#ifndef NETWORKFS_EMULATOR_H
#define NETWORKFS_EMULATOR_H

#include "../proto/proto.h"
#include "../networkfs.h"

struct fuse_operations *emulate_networkfs_oper (struct proto_operations *proto_oper);

#endif /* NETWORKFS_EMULATOR_H */
