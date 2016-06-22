# HARRY (Highly Available Redundant Routing Yard)

Description:

The unending expansion of Internet-enabled services drives demand for higher
speeds and more stringent reliability capabilities in the commercial Internet. Network
carriers, driven by these customer demands, increasingly acquire and deploy ever
larger routers with an ever growing feature set. The software however, is not currently
structured adequately to preserve Non-Stop Operation and is sometimes deployed in
non-redundant software configurations.

In this project, we look at current Routing Frameworks and address the
growing reliability concerns of Future Internet Architectures. We propose and implement
a new distributed architecture for next generation core routers by implementing
N-Modular Redundancy for Control Plane Processes. We use a distributed key-value
data store as a substrate for High Availability (HA) operation and synchronization
with different consistency requirements. A new platform is proposed, with many characteristics
similar to Software Defined Network, on which the framework is deployed.

Our work looks at the feasibility of Non-Stop Routing System Design, where
a router can continue to process both control and data messages, even with multiple
concurrent software and hardware failures. A thorough discussion on the framework,
components, and capabilities is given, and an estimation of overhead, fault recovery
and responsiveness is presented. We argue that even though our work is focused in the
context of routing, it also applies to any use-case where Non-Stop Real-Time response
is required.
