AUTOMAKE_OPTIONS = foreign subdir-objects

AM_CPPFLAGS = -fPIC
AM_CFLAGS = -Wall

nobase_include_HEADERS = \
	bofuss/action_set.h \
	bofuss/compiler.h \
	bofuss/crc32.h \
	bofuss/csum.h \
	bofuss/datapath.h \
	bofuss/dp_actions.h \
	bofuss/dp_buffers.h \
	bofuss/dp_capabilities.h \
	bofuss/dp_control.h \
	bofuss/dpctl.h \
	bofuss/dp_exp.h \
	bofuss/dp_ports.h \
	bofuss/dynamic-string.h \
	bofuss/flow_entry.h \
	bofuss/flow_table.h \
	bofuss/group_entry.h \
	bofuss/group_table.h \
	bofuss/hash.h \
	bofuss/hmap.h \
	bofuss/ipv6-util.h \
	bofuss/list.h \
	bofuss/match_std.h \
	bofuss/meter_entry.h \
	bofuss/meter_table.h \
	bofuss/ofl-actions.h \
	bofuss/ofl-err.h \
	bofuss/ofl-exp.h \
	bofuss/ofl-messages.h \
	bofuss/ofl-print.h \
	bofuss/ofl-structs.h \
	bofuss/ofl-utils.h \
	bofuss/ofpbuf.h \
	bofuss/openflow.h \
	bofuss/oxm-match.def \
	bofuss/oxm-match.h \
	bofuss/packet.h \
	bofuss/packet_handle_std.h \
	bofuss/packet-utils.h \
	bofuss/pipeline.h \
	bofuss/random.h \
	bofuss/timeval.h \
	bofuss/util.h \
	bofuss/vlog.h \
	bofuss/vlog-modules.def

lib_LIBRARIES = libbofuss.a

libbofuss_a_SOURCES = \
	sources/lib/crc32.c \
	sources/lib/csum.c \
	sources/lib/dynamic-string.c \
	sources/lib/hash.c \
	sources/lib/hmap.c \
	sources/lib/ipv6-util.c \
	sources/lib/list.c \
	sources/lib/ofpbuf.c \
	sources/lib/random.c \
	sources/lib/timeval.c \
	sources/lib/util.c \
	sources/lib/vlog.c \
	sources/oflib/ofl-actions-pack.c \
	sources/oflib/ofl-actions-print.c \
	sources/oflib/ofl-actions-unpack.c \
	sources/oflib/ofl-actions.c \
	sources/oflib/ofl-exp.c \
	sources/oflib/ofl-messages-pack.c \
	sources/oflib/ofl-messages-print.c \
	sources/oflib/ofl-messages-unpack.c \
	sources/oflib/ofl-messages.c \
	sources/oflib/ofl-print.c \
	sources/oflib/ofl-structs-match.c \
	sources/oflib/ofl-structs-pack.c \
	sources/oflib/ofl-structs-print.c \
	sources/oflib/ofl-structs-unpack.c \
	sources/oflib/ofl-structs.c \
	sources/oflib/oxm-match.c \
	sources/udatapath/action_set.c \
	sources/udatapath/datapath.c \
	sources/udatapath/dp_actions.c \
	sources/udatapath/dp_buffers.c \
	sources/udatapath/dp_control.c \
	sources/udatapath/dp_exp.c \
	sources/udatapath/dp_ports.c \
	sources/udatapath/flow_entry.c \
	sources/udatapath/flow_table.c \
	sources/udatapath/group_entry.c \
	sources/udatapath/group_table.c \
	sources/udatapath/match_std.c \
	sources/udatapath/meter_entry.c \
	sources/udatapath/meter_table.c \
	sources/udatapath/packet_handle_std.c \
	sources/udatapath/packet.c \
	sources/udatapath/pipeline.c \
	sources/utilities/dpctl.c
