module Ina229Bench {
  @ Bench-side caller of the manager's typed ports. F Prime 4 deployments use
  @ direct port calls, so Main reaches the manager through this component.
  passive component BenchClient {
    output port configure: Ina229.Configure
    output port read: Ina229.Read
  }
}
