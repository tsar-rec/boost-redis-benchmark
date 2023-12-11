# boost-redis-benchmark
A multi-write benchmark for Boost redis client

This benchmarks makes a relatively large number of `HSET` requests to the Redis server using `Boost.Redis` client.

## Requirements
* C++20 compiler (tested on `gcc` 12)
* `boost` (tested v1.83.0, built with SSL support)
* `boost::redis` installed to the same directory as `boost`.

## Build

```
g++ -std=c++20 batch_send_benchmark.cpp -o batch_send_benchmark -lboost_system -lssl -lcrypto
```

## Run

```
./batch_send_benchmark [n_req [payload_size]]
```

where
* `n_req` is a number of `HSET` requests sent.
* `payload_size` is a number of characters in a payload string.

## Details

All the boilerplate aside, this benchmark sends the requests to Redis in two ways. In the `separate` part, in function `runSeparateRequestsOnce()`, all the requests are sent for execution independently. In the `combined` part, in function `runCombinedRequestsOnce()`, all the `HSET` commands are put into the same request, which is then sent for execution.

The benchmark does three runs in a row: the first one contains only separate part, the second one contains only conbimed part, and the last one contains both parts.
