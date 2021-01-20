// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "leveldb/table.h"

#include <map>
#include <string>
#include "db/dbformat.h"
#include "db/memtable.h"
#include "db/write_batch_internal.h"
#include "leveldb/db.h"
#include "leveldb/env.h"
#include "leveldb/iterator.h"
#include "leveldb/table_builder.h"
#include "table/block.h"
#include "table/block_builder.h"
#include "table/format.h"
#include "util/random.h"
#include "util/testharness.h"
#include "util/testutil.h"

namespace leveldb {

// Return reverse of "key".
// Used to test non-lexicographic comparators.
static std::string Reverse(const Slice& key) {
  std::string str(key.ToString());
  std::string rev("");
  for (std::string::reverse_iterator rit = str.rbegin();
       rit != str.rend(); ++rit) {
    rev.push_back(*rit);
  }
  return rev;
}

namespace {
class ReverseKeyComparator : public Comparator {
 public:
  virtual const char* Name() const {
    return "leveldb.ReverseBytewiseComparator";
  }

  virtual int Compare(const Slice& a, const Slice& b) const {
    return BytewiseComparator()->Compare(Reverse(a), Reverse(b));
  }

  virtual void FindShortestSeparator(
      std::string* start,
      const Slice& limit) const {
    std::string s = Reverse(*start);
    std::string l = Reverse(limit);
    BytewiseComparator()->FindShortestSeparator(&s, l);
    *start = Reverse(s);
  }

  virtual void FindShortSuccessor(std::string* key) const {
    std::string s = Reverse(*key);
    BytewiseComparator()->FindShortSuccessor(&s);
    *key = Reverse(s);
  }
};
}  // namespace
static ReverseKeyComparator reverse_key_comparator;

static void Increment(const Comparator* cmp, std::string* key) {
  if (cmp == BytewiseComparator()) {
    key->push_back('\0');
  } else {
    assert(cmp == &reverse_key_comparator);
    std::string rev = Reverse(*key);
    rev.push_back('\0');
    *key = Reverse(rev);
  }
}

// An STL comparator that uses a Comparator
namespace {
struct STLLessThan {
  const Comparator* cmp;

  STLLessThan() : cmp(BytewiseComparator()) { }
  STLLessThan(const Comparator* c) : cmp(c) { }
  bool operator()(const std::string& a, const std::string& b) const {
    return cmp->Compare(Slice(a), Slice(b)) < 0;
  }
};
}  // namespace

class StringSink: public WritableFile {
 public:
  ~StringSink() { }

  const std::string& contents() const { return contents_; }

  virtual Status Close() { return Status::OK(); }
  virtual Status Flush() { return Status::OK(); }
  virtual Status Sync() { return Status::OK(); }

  virtual Status Append(const Slice& data) {
    contents_.append(data.data(), data.size());
    return Status::OK();
  }

 private:
  std::string contents_;
};


class StringSource: public RandomAccessFile {
 public:
  StringSource(const Slice& contents)
      : contents_(contents.data(), contents.size()) {
  }

  virtual ~StringSource() { }

  uint64_t Size() const { return contents_.size(); }

  virtual Status Read(uint64_t offset, size_t n, Slice* result,
                       char* scratch) const {
    if (offset > contents_.size()) {
      return Status::InvalidArgument("invalid Read offset");
    }
    if (offset + n > contents_.size()) {
      n = contents_.size() - offset;
    }
    memcpy(scratch, &contents_[offset], n);
    *result = Slice(scratch, n);
    return Status::OK();
  }

 private:
  std::string contents_;
};

typedef std::map<std::string, std::string, STLLessThan> KVMap;

// Helper class for tests to unify the interface between
// BlockBuilder/TableBuilder and Block/Table.
class Constructor {
 public:
  explicit Constructor(const Comparator* cmp) : data_(STLLessThan(cmp)) { }
  virtual ~Constructor() { }

  void Add(const std::string& key, const Slice& value) {
    data_[key] = value.ToString();
  }

  // Finish constructing the data structure with all the keys that have
  // been added so far.  Returns the keys in sorted order in "*keys"
  // and stores the key/value pairs in "*kvmap"
  void Finish(const Options& options,
              std::vector<std::string>* keys,
              KVMap* kvmap) {
    *kvmap = data_;
    keys->clear();
    for (KVMap::const_iterator it = data_.begin();
         it != data_.end();
         ++it) {
      keys->push_back(it->first);
    }
    data_.clear();
    Status s = FinishImpl(options, *kvmap);
    ASSERT_TRUE(s.ok()) << s.ToString();
  }

  // Construct the data structure from the data in "data"
  virtual Status FinishImpl(const Options& options, const KVMap& data) = 0;

  virtual Iterator* NewIterator() const = 0;

  virtual const KVMap& data() { return data_; }

  virtual DB* db() const { return NULL; }  // Overridden in DBConstructor

 private:
  KVMap data_;
};

class BlockConstructor: public Constructor {
 public:
  explicit BlockConstructor(const Comparator* cmp)
      : Constructor(cmp),
        comparator_(cmp),
        block_(NULL) { }
  ~BlockConstructor() {
    delete block_;
  }
  virtual Status FinishImpl(const Options& options, const KVMap& data) {
    delete block_;
    block_ = NULL;
    BlockBuilder builder(&options);

    for (KVMap::const_iterator it = data.begin();
         it != data.end();
         ++it) {
      builder.Add(it->first, it->second);
    }
    // Open the block
    data_ = builder.Finish().ToString();
    BlockContents contents;
    contents.data = data_;
    contents.cachable = false;
    contents.heap_allocated = false;
    block_ = new Block(contents);
    return Status::OK();
  }
  virtual Iterator* NewIterator() const {
    return block_->NewIterator(comparator_);
  }

 private:
  const Comparator* comparator_;
  std::string data_;
  Block* block_;

  BlockConstructor();
};

class TableConstructor: public Constructor {
 public:
  TableConstructor(const Comparator* cmp)
      : Constructor(cmp),
        source_(NULL), table_(NULL) {
  }
  ~TableConstructor() {
    Reset();
  }
  virtual Status FinishImpl(const Options& options, const KVMap& data) {
    Reset();
    StringSink sink;
    TableBuilder builder(options, &sink);

    for (KVMap::const_iterator it = data.begin();
         it != data.end();
         ++it) {
      builder.Add(it->first, it->second);
      ASSERT_TRUE(builder.status().ok());
    }
    Status s = builder.Finish();
    ASSERT_TRUE(s.ok()) << s.ToString();

    ASSERT_EQ(sink.contents().size(), builder.FileSize());

    // Open the table
    source_ = new StringSource(sink.contents());
    Options table_options;
    table_options.comparator = options.comparator;
    return Table::Open(table_options, source_, sink.contents().size(), &table_);
  }

  virtual Iterator* NewIterator() const {
    return table_->NewIterator(ReadOptions());
  }

  uint64_t ApproximateOffsetOf(const Slice& key) const {
    return table_->ApproximateOffsetOf(key);
  }

 private:
  void Reset() {
    delete table_;
    delete source_;
    table_ = NULL;
    source_ = NULL;
  }

  StringSource* source_;
  Table* table_;

  TableConstructor();
};

// A helper class that converts internal format keys into user keys
class KeyConvertingIterator: public Iterator {
 public:
  explicit KeyConvertingIterator(Iterator* iter) : iter_(iter) { }
  virtual ~KeyConvertingIterator() { delete iter_; }
  virtual bool Valid() const { return iter_->Valid(); }
  virtual void Seek(const Slice& target) {
    ParsedInternalKey ikey(target, kMaxSequenceNumber, kTypeValue);
    std::string encoded;
    AppendInternalKey(&encoded, ikey);
    iter_->Seek(encoded);
  }
  virtual void SeekToFirst() { iter_->SeekToFirst(); }
  virtual void SeekToLast() { iter_->SeekToLast(); }
  virtual void Next() { iter_->Next(); }
  virtual void Prev() { iter_->Prev(); }

  virtual Slice key() const {
    assert(Valid());
    ParsedInternalKey key;
    if (!ParseInternalKey(iter_->key(), &key)) {
      status_ = Status::Corruption("malformed internal key");
      return Slice("corrupted key");
    }
    return key.user_key;
  }

  virtual Slice value() const { return iter_->value(); }
  virtual Status status() const {
    return status_.ok() ? iter_->status() : status_;
  }

 private:
  mutable Status status_;
  Iterator* iter_;

  // No copying allowed
  KeyConvertingIterator(const KeyConvertingIterator&);
  void operator=(const KeyConvertingIterator&);
};

class MemTableConstructor: public Constructor {
 public:
  explicit MemTableConstructor(const Comparator* cmp)
      : Constructor(cmp),
        internal_comparator_(cmp) {
    memtable_ = new MemTable(internal_comparator_);
    memtable_->Ref();
  }
  ~MemTableConstructor() {
    memtable_->Unref();
  }
  virtual Status FinishImpl(const Options& options, const KVMap& data) {
    memtable_->Unref();
    memtable_ = new MemTable(internal_comparator_);
    memtable_->Ref();
    int seq = 1;
    for (KVMap::const_iterator it = data.begin();
         it != data.end();
         ++it) {
      memtable_->Add(seq, kTypeValue, it->first, it->second);
      seq++;
    }
    return Status::OK();
  }
  virtual Iterator* NewIterator() const {
    return new KeyConvertingIterator(memtable_->NewIterator());
  }

 private:
  InternalKeyComparator internal_comparator_;
  MemTable* memtable_;
};

class DBConstructor: public Constructor {
 public:
  explicit DBConstructor(const Comparator* cmp)
      : Constructor(cmp),
        comparator_(cmp) {
    db_ = NULL;
    NewDB();
  }
  ~DBConstructor() {
    delete db_;
  }
  virtual Status FinishImpl(const Options& options, const KVMap& data) {
    delete db_;
    db_ = NULL;
    NewDB();
    for (KVMap::const_iterator it = data.begin();
         it != data.end();
         ++it) {
      WriteBatch batch;
      batch.Put(it->first, it->second);
      ASSERT_TRUE(db_->Write(WriteOptions(), &batch).ok());
    }
    return Status::OK();
  }
  virtual Iterator* NewIterator() const {
    return db_->NewIterator(ReadOptions());
  }

  virtual DB* db() const { return db_; }

 private:
  void NewDB() {
    std::string name = test::TmpDir() + "/table_testdb";

    Options options;
    options.comparator = comparator_;
    Status status = DestroyDB(name, options);
    ASSERT_TRUE(status.ok()) << status.ToString();

    options.create_if_missing = true;
    options.error_if_exists = true;
    options.write_buffer_size = 10000;  // Something small to force merging
    status = DB::Open(options, name, &db_);
    ASSERT_TRUE(status.ok()) << status.ToString();
  }

  const Comparator* comparator_;
  DB* db_;
};

enum TestType {
  TABLE_TEST,
  BLOCK_TEST,
  MEMTABLE_TEST,
  DB_TEST
};

struct TestArgs {
  TestType type;
  bool reverse_compare;
  int restart_interval;
};

static const TestArgs kTestArgList[] = {
  { TABLE_TEST, false, 16 },
  { TABLE_TEST, false, 1 },
  { TABLE_TEST, false, 1024 },
  { TABLE_TEST, true, 16 },
  { TABLE_TEST, true, 1 },
  { TABLE_TEST, true, 1024 },

  { BLOCK_TEST, false, 16 },
  { BLOCK_TEST, false, 1 },
  { BLOCK_TEST, false, 1024 },
  { BLOCK_TEST, true, 16 },
  { BLOCK_TEST, true, 1 },
  { BLOCK_TEST, true, 1024 },

  // Restart interval does not matter for memtables
  { MEMTABLE_TEST, false, 16 },
  { MEMTABLE_TEST, true, 16 },

  // Do not bother with restart interval variations for DB
  { DB_TEST, false, 16 },
  { DB_TEST, true, 16 },
};
static const int kNumTestArgs = sizeof(kTestArgList) / sizeof(kTestArgList[0]);

class Harness {
 public:
  Harness() : constructor_(NULL) { }

  void Init(const TestArgs& args) {
    delete constructor_;
    constructor_ = NULL;
    options_ = Options();

    options_.block_restart_interval = args.restart_interval;
    // Use shorter block size for tests to exercise block boundary
    // conditions more.
    options_.block_size = 256;
    if (args.reverse_compare) {
      options_.comparator = &reverse_key_comparator;
    }
    switch (args.type) {
      case TABLE_TEST:
        constructor_ = new TableConstructor(options_.comparator);
        break;
      case BLOCK_TEST:
        constructor_ = new BlockConstructor(options_.comparator);
        break;
      case MEMTABLE_TEST:
        constructor_ = new MemTableConstructor(options_.comparator);
        break;
      case DB_TEST:
        constructor_ = new DBConstructor(options_.comparator);
        break;
    }
  }

  ~Harness() {
    delete constructor_;
  }

  void Add(const std::string& key, const std::string& value) {
    constructor_->Add(key, value);
  }

  void Test(Random* rnd) {
    std::vector<std::string> keys;
    KVMap data;
    constructor_->Finish(options_, &keys, &data);

    TestForwardScan(keys, data);
    TestBackwardScan(keys, data);
    TestRandomAcc