// Copyright 2020 CryptoGarage
/**
 * @file cfdapi_ledger.cpp
 *
 * @brief implements ledger support function on cfd-api.
 */
#include <string>
#include <vector>

#include "cfd/cfd_common.h"
#include "cfd/cfd_elements_transaction.h"
#include "cfd/cfdapi_ledger.h"
#include "cfdcore/cfdcore_common.h"
#include "cfdcore/cfdcore_elements_transaction.h"
#include "cfdcore/cfdcore_logger.h"
#include "cfdcore/cfdcore_transaction_common.h"
#include "cfdcore/cfdcore_util.h"

using cfd::core::ByteData;
using cfd::core::CfdError;
using cfd::core::CfdException;

using cfd::core::logger::info;
using cfd::core::logger::warn;

#ifndef CFD_DISABLE_ELEMENTS
using cfd::ConfidentialTransactionContext;
using cfd::core::ConfidentialTransaction;
using cfd::core::ConfidentialTxInReference;
using cfd::core::ConfidentialTxOutReference;
#endif  // CFD_DISABLE_ELEMENTS

// =============================================================================
// internal c-api
// =============================================================================
namespace cfd {
namespace api {

#ifndef CFD_DISABLE_ELEMENTS

ByteData LedgerApi::Serialize(
    const ConfidentialTransactionContext& tx,
    const std::vector<LedgerMetaDataStackItem>& metadata_stack,
    bool skip_witness, bool is_authorization) const {
  static auto serialize_input_function =
      [](const ConfidentialTxInReference& txin,
         bool is_authorization) -> ByteData {
    ByteData txin_bytes;
    uint32_t vout = txin.GetVout();
    std::vector<uint8_t> byte_data(sizeof(vout));
    memcpy(byte_data.data(), &vout, byte_data.size());
    txin_bytes = txin.GetTxid().GetData();
    txin_bytes.Push(ByteData(byte_data));
    if (!is_authorization) {
      txin_bytes.Push(txin.GetUnlockingScript().GetData().Serialize());
    }
    uint32_t sequence = txin.GetSequence();
    memcpy(byte_data.data(), &sequence, byte_data.size());
    txin_bytes.Push(ByteData(byte_data));
    return txin_bytes;
  };

  static auto serialize_output_function =
      [](const ConfidentialTxOutReference& txout, bool is_authorization,
         const LedgerMetaDataStackItem& metadata) -> ByteData {
    ByteData txout_bytes;
    if (!is_authorization) {
      txout_bytes = txout_bytes.Concat(
          txout.GetAsset().GetData(), txout.GetConfidentialValue().GetData(),
          txout.GetNonce().GetData(),
          txout.GetLockingScript().GetData().Serialize());
    } else if (!txout.GetConfidentialValue().HasBlinding()) {
      txout_bytes = txout_bytes.Concat(
          ByteData(txout.GetAsset().GetHex()),
          ByteData(txout.GetConfidentialValue().GetHex().substr(2)),
          txout.GetLockingScript().GetData().Serialize());
    } else {
      txout_bytes = txout_bytes.Concat(
          ByteData(metadata.metadata1), ByteData(metadata.metadata2),
          txout.GetLockingScript().GetData().Serialize());
    }
    return txout_bytes;
  };

  bool has_witness = (tx.HasWitness() && !skip_witness && !is_authorization);
  ByteData data;
  ByteData temp_data;

  // version
  int32_t version = tx.GetVersion();
  std::vector<uint8_t> byte_data(sizeof(version));
  memcpy(byte_data.data(), &version, byte_data.size());
  data.Push(ByteData(byte_data));

  // marker & flag (equivalent to bitcoin format)
  if (has_witness) {
    data.Push(ByteData("0001"));
  }

  uint32_t txin_count = tx.GetTxInCount();
  data.Push(ByteData::GetVariableInt(txin_count));
  for (uint32_t index = 0; index < txin_count; ++index) {
    temp_data = serialize_input_function(tx.GetTxIn(index), is_authorization);
    data.Push(temp_data);
  }

  LedgerMetaDataStackItem empty_item;
  uint32_t txout_count = tx.GetTxOutCount();
  data.Push(ByteData::GetVariableInt(txout_count));
  for (uint32_t index = 0; index < txout_count; ++index) {
    auto txout = tx.GetTxOut(index);
    if (metadata_stack.size() > index) {
      temp_data = serialize_output_function(
          txout, is_authorization, metadata_stack.at(index));
    } else if (
        is_authorization && txout.GetConfidentialValue().HasBlinding()) {
      warn(CFD_LOG_SOURCE, "metadata empty.");
      throw CfdException(
          CfdError::kCfdIllegalArgumentError,
          "Failed to metadata. not set metadata with authrization.");
    } else {
      temp_data =
          serialize_output_function(txout, is_authorization, empty_item);
    }
    data.Push(temp_data);
  }

  if (has_witness) {
    // locktime
    uint32_t locktime = tx.GetLockTime();
    memcpy(byte_data.data(), &locktime, byte_data.size());
    data.Push(ByteData(byte_data));

    // issue rangeproof, token rangeproof, witness, pegin witness
    // surjectionproof, rangeproof
    // FIXME(k-matsuzawa): not implement
    throw CfdException(
        CfdError::kCfdIllegalStateError,
        "Witness serialization is not implemented.");
  }

  return data;
}
#endif  // CFD_DISABLE_ELEMENTS

}  // namespace api
}  // namespace cfd
