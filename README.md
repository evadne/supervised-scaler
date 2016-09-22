# Supervised Scaler

The Supervised Scaler is an Elixir/Phoenix project made to support my lightning talk at Elixir London 2016. It is laid out thusly:

1.  A simple Phoenix application, `scaler`, which generates thumbnails using a local dependency.
2.  The local dependency, `scaleserver` which contains an Elixir wrapper around a C server, which calls libVIPS.

## References

* [Elixir Native Interoperability – Ports vs. NIFs](https://spin.atomicobject.com/2015/03/16/elixir-native-interoperability-ports-vs-nifs/)
* [erlexec](https://github.com/saleyn/erlexec)
* [exexec](https://github.com/antipax/exexec)
