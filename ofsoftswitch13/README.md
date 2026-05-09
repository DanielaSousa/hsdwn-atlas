# BOFUSS library for OFSwitch13

This is an [OpenFlow 1.3][ofp13] compatible user-space software switch, forked from the original [Basic OpenFlow User Space Software Switch (BOFUSS)][cpqdofs13] and modified to proper integration with the [OFSwitch13 module][ofswitch13] for the [ns-3 Network Simulator][ns-3]. The `master` branch does not modify the original switch implementation, which is currently maintained in the original repository and regularly synced to this one. The modified `ns3lib` branch includes only the necessary files for building the BOFUSS library and integrating it with the OFSwitch13 module. 
<!-- The `ns3lib-gtpu` branch also enhances the software switch with GTP-U OXM support. -->

# Contribute
Please, consider submitting your bug reports to the original [BOFUSS project][cpqdofs13].

# License
This code is released under the BSD license (BSD-like for code from the original Stanford switch).

# Acknowledgments
Thanks to the original [BOFUSS project][cpqdofs13] contributors. Special thanks to [Eder Leão Fernandes][ederlf], for contributing to the integration between this software switch and the ns-3 simulator.

# Contact
Feel free to subscribe to [our mailing list at Google groups][group] and provide some feedback, give us suggestions, interact with other users, or to just say hello!

[ofp13]: https://www.opennetworking.org/sdn-resources/technical-library
[cpqdofs13]: https://github.com/CPqD/ofsoftswitch13
[ofswitch13]: https://github.com/ljerezchaves/ofswitch13
[ns-3]: https://www.nsnam.org
[ederlf]: https://github.com/ederlf
[group]: https://groups.google.com/forum/#!forum/ofswitch13-users
