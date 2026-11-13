# Connection-Trace-Studio Ruby SDK

`gem install connection-trace-studio`

Windows x64 Ruby3.1+.

```ruby
require "connection_trace_studio"
ConnectionTraceStudioSDK::Client.open { |client| p client.call("status") }
```

[API and operation schema](docs/API.md). Matching driver installed separately.
