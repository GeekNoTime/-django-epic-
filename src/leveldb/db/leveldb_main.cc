
// Copyright (c) 2012 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include <stdio.h>
#include "db/dbformat.h"
#include "db/filename.h"
#include "db/log_reader.h"
#include "db/version_edit.h"
#include "db/write_batch_internal.h"
#include "leveldb/env.h"
#include "leveldb/iterator.h"
#include "leveldb/options.h"
#include "leveldb/status.h"
#include "leveldb/table.h"
#include "leveldb/write_batch.h"
#include "util/logging.h"

namespace leveldb {

namespace {

bool GuessType(const std::string& fname, FileType* type) {
  size_t pos = fname.rfind('/');
  std::string basename;
  if (pos == std::string::npos) {
    basename = fname;
  } else {
    basename = std::string(fname.data() + pos + 1, fname.size() - pos - 1);
  }
  uint64_t ignored;
  return ParseFileName(basename, &ignored, type);
}

// Notified when log reader encounters corruption.
class CorruptionReporter : public log::Reader::Reporter {
 public:
  virtual void Corruption(size_t bytes, const Status& status) {
    printf("corruption: %d bytes; %s\n",
            static_cast<int>(bytes),
            status.ToString().c_str());
  }
};

// Print contents of a log file. (*func)() is called on every record.
bool PrintLogContents(Env* env, const std::string& fname,
                      void (*func)(Slice)) {
  SequentialFile* file;
  Status s = env->NewSequentialFile(fname, &file);
  if (!s.ok()) {
    fprintf(stderr, "%s\n", s.ToString().c_str());
    return false;
  }
  CorruptionReporter reporter;
  log::Reader reader(file, &reporter, true, 0);
  Slice record;
  std::string scratch;
  while (reader.ReadRecord(&record, &scratch)) {
    printf("--- offset %llu; ",
           static_cast<unsigned long long>(reader.LastRecordOffset()));
    (*func)(record);
  }
  delete file;
  return true;
}

// Called on every item found in a WriteBatch.
class WriteBatchItemPrinter : public WriteBatch::Handler {
 public:
  uint64_t offset_;
  uint64_t sequence_;

  virtual void Put(const Slice& key, const Slice& value) {
    printf("  put '%s' '%s'\n",
           EscapeString(key).c_str(),
           EscapeString(value).c_str());
  }
  virtual void Delete(const Slice& key) {
    printf("  del '%s'\n",
           EscapeString(key).c_str());
  }
};


// Called on every log record (each one of which is a WriteBatch)
// found in a kLogFile.
static void WriteBatchPrinter(Slice record) {
  if (record.size() < 12) {
    printf("log record length %d is too small\n",
           static_cast<int>(record.size()));
    return;
  }
  WriteBatch batch;
  WriteBatchInternal::SetContents(&batch, record);
  printf("sequence %llu\n",
         static_cast<unsigned long long>(WriteBatchInternal::Sequence(&batch)));
  WriteBatchItemPrinter batch_item_printer;
  Status s = batch.Iterate(&batch_item_printer);
  if (!s.ok()) {
    printf("  error: %s\n", s.ToString().c_str());
  }
}

bool DumpLog(Env* env, const std::string& fname) {
  return PrintLogContents(env, fname, WriteBatchPrinter);
}

// Called on every log record (each one of which is a WriteBatch)
// found in a kDescriptorFile.
static void VersionEditPrinter(Slice record) {
  VersionEdit edit;
  Status s = edit.DecodeFrom(record);
  if (!s.ok()) {
    printf("%s\n", s.ToString().c_str());
    return;
  }
  printf("%s", edit.DebugString().c_str());
}

bool DumpDescriptor(Env* env, const std::string& fname) {
  return PrintLogContents(env, fname, VersionEditPrinter);
}

bool DumpTable(Env* env, const std::string& fname) {
  uint64_t file_size;
  RandomAccessFile* file = NULL;
  Table* table = NULL;
  Status s = env->GetFileSize(fname, &file_size);
  if (s.ok()) {
    s = env->NewRandomAccessFile(fname, &file);
  }
  if (s.ok()) {
    // We use the default comparator, which may or may not match the
    // comparator used in this database. However this should not cause
    // problems since we only use Table operations that do not require
    // any comparisons.  In particular, we do not call Seek or Prev.
    s = Table::Open(Options(), file, file_size, &table);
  }
  if (!s.ok()) {
    fprintf(stderr, "%s\n", s.ToString().c_str());