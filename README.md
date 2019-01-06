# asionet
A header-only cross-platform C++ network library based on boost asio.

Guidelines:
- don't make assumptions about the lifetime of input data (avoid shared_ptr arguments)