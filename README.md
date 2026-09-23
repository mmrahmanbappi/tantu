# tantu

**Weaving the web in C, from chip to cloud.**

Website: https://mmrahmanbappi.github.io/tantu/

*tantu* is an old Sanskrit word for "thread" or "fiber". A web is woven from many small threads, and this framework is built the same way: one small, focused module at a time.

> **Status: early stage / planning.** There is no usable code yet. This is the perfect time to get involved and help shape the design.

## Vision

One C API that runs everywhere:

- **Servers:** a fast, lightweight HTTP framework
- **Microcontrollers:** the same API on devices like the ESP32
- **Edge & browser:** compiled to WebAssembly (WASI)
- **AI-ready:** streaming, LLM gateways, and vector search built in

## How it grows

tantu is extracted from real projects. Each project on the roadmap produces a reusable module, and those modules together become the framework.

| Phase | Projects | Module(s) produced |
|---|---|---|
| 1. Foundation | HTTP server, JSON API toolkit, URL router | core (event loop + HTTP parser), json, router |
| 2. Real-time | WebSocket chat, live dashboard (SSE), pub/sub broker | websocket, stream, bus |
| 3. AI | LLM API gateway, local model server, vector search, document search (RAG) | client/proxy, serve, vector |
| 4. Edge & WebAssembly | WASI build, browser target, edge cache | portability layer, wasm, cache |
| 5. IoT | ESP32 web server, sensor time-series API, MQTT to HTTP bridge | embedded mode, timeseries, adapters |
| 6. Security | Auth server (JWT/OAuth2/passkeys), WAF & rate limiter, automatic HTTPS | auth, guard, tls |
| 7. Modern protocols | HTTP/3 (QUIC) server | h3 transport |
| 8. Developer experience | CLI scaffolding, hot reload, Python/Node/PHP bindings | cli, dev tools, bindings |

## Principles

- **Small and readable:** every module should be understandable in an afternoon.
- **Secure by default:** memory safety is taken seriously. Code is tested with sanitizers (ASan/UBSan) and fuzzing.
- **Portable:** plain C (C11), minimal dependencies.
- **Modular:** use only the threads you need.

## Contributing

Contributions of all kinds are welcome: code, design ideas, documentation, and reviews. See [CONTRIBUTING.md](CONTRIBUTING.md).

## License

[MIT](LICENSE)
