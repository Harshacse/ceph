// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*- 
// vim: ts=8 sw=2 smarttab
/*
 * Ceph - scalable distributed file system
 *
 * Copyright (C) 2004-2006 Sage Weil <sage@newdream.net>
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software 
 * Foundation.  See file COPYING.
 * 
 */

#include "InoTable.h"
#include "MDS.h"

#include "include/types.h"

#include "config.h"

#define DOUT_SUBSYS mds
#undef dout_prefix
#define dout_prefix *_dout << dbeginl << "mds" << mds->get_nodeid() << "." << table_name << ": "

void InoTable::init_inode()
{
  ino = MDS_INO_IDS_OFFSET + mds->get_nodeid();
  layout = g_default_file_layout;
}

void InoTable::reset_state()
{
  // use generic range. FIXME THIS IS CRAP
  free.clear();
  //#ifdef __LP64__
  uint64_t start = (uint64_t)(mds->get_nodeid()+1) << 40;
  uint64_t end = ((uint64_t)(mds->get_nodeid()+2) << 40) - 1;
  //#else
  //# warning this looks like a 32-bit system, using small inode numbers.
  //  uint64_t start = (uint64_t)(mds->get_nodeid()+1) << 25;
  //  uint64_t end = ((uint64_t)(mds->get_nodeid()+2) << 25) - 1;
  //#endif
  free.insert(start, end);

  projected_free.m = free.m;
}

inodeno_t InoTable::project_alloc_id(inodeno_t id) 
{
  dout(10) << "project_alloc_id " << id << " to " << projected_free << "/" << free << dendl;
  assert(is_active());
  if (!id)
    id = projected_free.start();
  projected_free.erase(id);
  ++projected_version;
  return id;
}
void InoTable::apply_alloc_id(inodeno_t id)
{
  dout(10) << "apply_alloc_id " << id << " to " << projected_free << "/" << free << dendl;
  free.erase(id);
  ++version;
}

void InoTable::project_alloc_ids(interval_set<inodeno_t>& ids, int want) 
{
  dout(10) << "project_alloc_ids " << ids << " to " << projected_free << "/" << free << dendl;
  assert(is_active());
  while (want > 0) {
    inodeno_t start = projected_free.start();
    inodeno_t end = projected_free.end_after(start);
    inodeno_t num = end - start;
    if (num > (inodeno_t)want)
      num = want;
    projected_free.erase(start, num);
    ids.insert(start, num);
    want -= num;
  }
  ++projected_version;
}
void InoTable::apply_alloc_ids(interval_set<inodeno_t>& ids)
{
  dout(10) << "apply_alloc_ids " << ids << " to " << projected_free << "/" << free << dendl;
  free.subtract(ids);
  ++version;
}


void InoTable::project_release_ids(interval_set<inodeno_t>& ids) 
{
  dout(10) << "project_release_ids " << ids << " to " << projected_free << "/" << free << dendl;
  projected_free.insert(ids);
  ++projected_version;
}
void InoTable::apply_release_ids(interval_set<inodeno_t>& ids) 
{
  dout(10) << "apply_release_ids " << ids << " to " << projected_free << "/" << free << dendl;
  free.insert(ids);
  ++version;
}


//

void InoTable::replay_alloc_id(inodeno_t id) 
{
  dout(10) << "replay_alloc_id " << id << dendl;
  free.erase(id);
  projected_free.erase(id);
  projected_version = ++version;
}
void InoTable::replay_alloc_ids(interval_set<inodeno_t>& ids) 
{
  dout(10) << "replay_alloc_ids " << ids << dendl;
  free.subtract(ids);
  projected_free.subtract(ids);
  projected_version = ++version;
}
void InoTable::replay_release_ids(interval_set<inodeno_t>& ids) 
{
  dout(10) << "replay_release_ids " << ids << dendl;
  free.insert(ids);
  projected_free.insert(ids);
  projected_version = ++version;
}

