#include <boost/asio/io_context.hpp>
#include <boost/redis.hpp>
#include <boost/redis/src.hpp>
#include <thread>

constexpr auto usage_str = R"(
Usage: batch_send_benchmark [n_req [payload_size]]
    nreq: number of requests sent in a batch, default 800.
    payload_size: a length of a payload string, default 3000.
)";
namespace redis
{
    namespace
    {

        class BatchSendBenchmark
        {
        public:
            BatchSendBenchmark(int n_req, int payload_size)
                : _conn(std::make_shared<boost::redis::connection>(_ctx)), _n_req(n_req), _payload_size(payload_size), _s("separate", "s.hash", n_req), _c("combined", "c.hash", 1)
            {
            }
            ~BatchSendBenchmark()
            {
                stop();
            }
            void run(bool run_s, bool run_c)
            {
                _thread = std::jthread([this]()
                                       { _ctx.run(); });
                // Turn off health check.
                boost::redis::config cfg;
                cfg.health_check_interval = std::chrono::seconds(0);
                _conn->async_run(cfg,
                                 {boost::redis::logger::level::debug},
                                 [_conn = this->_conn](boost::system::error_code ec)
                                 {
                                     if (ec)
                                     {
                                         std::cerr << "Error in async_run: " << ec.message() << std::endl;
                                     }
                                 });

                std::cout << "Running " << (run_s ? "separate" : "") << " " << (run_c ? "combined" : "") << std::endl;
                // Clear all the streams this BM is using.
                prepare();
                // run N requests to redis.
                if (run_s)
                    runSeparateRequests();
                // run one combined request.
                if (run_c)
                    runCombinedRequests();
                // wait for completion.
                waitForCompletion();
            }
            void stop()
            {
                _conn->cancel();
                _ctx.stop();
            }

        protected:
            struct Config
            {
                std::string name;
                std::string hash;
                int total;
                bool started{false};
                std::atomic<int> errors{0};
                std::atomic<int> done{0};

                Config(std::string_view config_name, std::string_view hash_name, int expected)
                    : name(config_name), hash(hash_name), total(expected)
                {
                }
                void clear()
                {
                    started = false;
                    errors = 0;
                    done = 0;
                }
                bool completed() const
                {
                    return !started || done == total;
                }
                std::string status() const
                {
                    if (!started)
                        return "";
                    return name + " " + std::to_string(done) + " of " + std::to_string(total) + " (with " +
                           std::to_string(errors) + " errors)";
                }
            };
            void prepare()
            {
                std::cout << "Starting BM" << std::endl;
                _s.clear();
                _c.clear();
            }
            void runSeparateRequests()
            {
                std::cout << "Separate stream BM start" << std::endl;
                runSeparateRequestsOnce();
                std::cout << "Separate stream BM ends" << std::endl;
            }
            void runSeparateRequestsOnce()
            {
                _s.started = true;
                for (int i = 0; i < _n_req; ++i)
                {
                    auto request = std::make_shared<boost::redis::request>();

                    request->push("HSET", _s.hash, std::to_string(i), payload());
                    _conn->async_exec(*request,
                                      boost::redis::ignore,
                                      [this, request](boost::system::error_code ec, size_t)
                                      {
                                          if (ec)
                                          {
                                              ++_s.errors;
                                          }
                                          ++_s.done;
                                      });
                }
            }
            void runCombinedRequests()
            {
                std::cout << "Combined stream BM start" << std::endl;
                runCombinedRequestsOnce();
                std::cout << "Combined stream BM ends" << std::endl;
            }
            void runCombinedRequestsOnce()
            {
                _c.started = true;
                auto request = std::make_shared<boost::redis::request>();
                for (int i = 0; i < _n_req; ++i)
                {
                    request->push("HSET", _c.hash, std::to_string(i), payload());
                }

                _conn->async_exec(*request,
                                  boost::redis::ignore,
                                  [this, request](boost::system::error_code ec, size_t)
                                  {
                                      if (ec)
                                      {
                                          ++_c.errors;
                                      }
                                      ++_c.done;
                                  });
            }
            void waitForCompletion()
            {
                while (!_s.completed() || !_c.completed())
                {
                    std::cout << "Waiting for completion: " << _s.status() << " " << _c.status() << "..." << std::endl;
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
                std::cout << "Finished: " << _s.status() << " " << _c.status() << std::endl;
            }
            std::string payload()
            {
                return std::string(_payload_size, 'a');
            }

        private:
            boost::asio::io_context _ctx{1};
            std::jthread _thread;
            std::shared_ptr<boost::redis::connection> _conn;
            int _n_req;
            int _payload_size;
            Config _s;
            Config _c;
        };

    }
} // namespace redis

int main(int argc, char *argv[])
{
    if (argc > 3)
    {
        std::cout << usage_str;
        return 1;
    }
    int payload_size = argc > 2 ? std::stoi(argv[2]) : 3000;
    int n_req = argc > 1 ? std::stoi(argv[1]) : 800;
    std::cout << "Creating BM with " << n_req << " requests and " << payload_size << "-long payload\n";
    {
        redis::BatchSendBenchmark b(n_req, payload_size);
        std::cout << "Starting BMs with separate requests..." << std::endl;
        b.run(true, false);
        std::cout << "BM with separate requests done" << std::endl;
        b.stop();
    }
    {
        redis::BatchSendBenchmark b(n_req, payload_size);
        std::cout << "Starting BMs with combined requests..." << std::endl;
        b.run(false, true);
        std::cout << "BM with combined requests done" << std::endl;
        b.stop();
    }
    {
        redis::BatchSendBenchmark b(n_req, payload_size);
        std::cout << "Starting BMs with both kinds of requests..." << std::endl;
        b.run(true, true);
        std::cout << "BM with both kinds of requests done" << std::endl;
        b.stop();
    }
    return 0;
}