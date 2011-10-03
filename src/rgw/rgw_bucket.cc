#include <errno.h>

#include <string>

#include "common/errno.h"
#include "rgw_access.h"

#include "rgw_bucket.h"
#include "rgw_tools.h"

static rgw_bucket pi_buckets(BUCKETS_POOL_NAME);

static string pool_name_prefix = "p";


int rgw_store_bucket_info(RGWBucketInfo& info)
{
  bufferlist bl;
  ::encode(info, bl);

  string unused;
  int ret = rgw_put_obj(unused, pi_buckets, info.bucket.name, bl.c_str(), bl.length());
  if (ret < 0)
    return ret;

  char bucket_char[16];
  snprintf(bucket_char, sizeof(bucket_char), ".%lld", (long long unsigned)info.bucket.bucket_id);
  string bucket_id_string(bucket_char);
  ret = rgw_put_obj(unused, pi_buckets, bucket_id_string, bl.c_str(), bl.length());

  RGW_LOG(0) << "rgw_store_bucket_info: bucket=" << info.bucket << " owner " << info.owner << dendl;
  return 0;
}

int rgw_get_bucket_info(string& bucket_name, RGWBucketInfo& info)
{
  bufferlist bl;

  int ret = rgw_get_obj(pi_buckets, bucket_name, bl);
  if (ret < 0) {
    if (ret != -ENOENT)
      return ret;

    info.bucket.name = bucket_name;
    info.bucket.pool = bucket_name; // for now
    return 0;
  }

  bufferlist::iterator iter = bl.begin();
  try {
    ::decode(info, iter);
  } catch (buffer::error& err) {
    RGW_LOG(0) << "ERROR: could not decode buffer info, caught buffer::error" << dendl;
    return -EIO;
  }

  RGW_LOG(0) << "rgw_get_bucket_info: bucket=" << info.bucket << " owner " << info.owner << dendl;

  return 0;
}

int rgw_get_bucket_info_id(uint64_t bucket_id, RGWBucketInfo& info)
{
  char bucket_char[16];
  snprintf(bucket_char, sizeof(bucket_char), ".%lld",
           (long long unsigned)bucket_id);
  string bucket_string(bucket_char);

  return rgw_get_bucket_info(bucket_string, info);
}

int rgw_create_bucket(std::string& id, string& bucket_name, rgw_bucket& bucket,
                      map<std::string, bufferlist>& attrs, bool exclusive, uint64_t auid)
{
  /* system bucket name? */
  if (bucket_name[0] == '.') {
    bucket.name = bucket_name;
    bucket.pool = bucket_name;
    return rgwstore->create_bucket(id, bucket, attrs, true, false, exclusive, auid);
  }

  int ret = rgwstore->select_bucket_placement(bucket_name, bucket);
  if (ret < 0)
     return ret;

  ret = rgwstore->create_bucket(id, bucket, attrs, false, true, exclusive, auid);

  if (ret < 0)
    return ret;

  RGWBucketInfo info;
  info.bucket = bucket;
  info.owner = id;
  ret = rgw_store_bucket_info(info);
  if (ret < 0) {
    RGW_LOG(0) << "failed to store bucket info, removing bucket" << dendl;
    rgwstore->delete_bucket(id, bucket, true);
    return ret;
  }

  return 0; 
}
