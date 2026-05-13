// Copyright (c) 2026 The Triangles developers.
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef TRIANGLES_CHAINDB_MIGRATE_H
#define TRIANGLES_CHAINDB_MIGRATE_H

#include <string>

// Migrate legacy LevelDB chain state from <datadir>/txleveldb to RocksDB in
// <datadir>/rocksdb. The source is never modified. Returns true when migration
// succeeds or when there is nothing to migrate.
bool MaybeMigrateLevelDbToRocksDb(bool fForce, std::string& strError);

#endif // TRIANGLES_CHAINDB_MIGRATE_H
