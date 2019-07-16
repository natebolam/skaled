/**
 * This file is generated by jsonrpcstub, DO NOT CHANGE IT MANUALLY!
 */

#ifndef JSONRPC_CPP_STUB_WEBTHREESTUBCLIENT_H_
#define JSONRPC_CPP_STUB_WEBTHREESTUBCLIENT_H_

#include <jsonrpccpp/client.h>

class WebThreeStubClient : public jsonrpc::Client {
public:
    WebThreeStubClient( jsonrpc::IClientConnector& conn,
        jsonrpc::clientVersion_t type = jsonrpc::JSONRPC_CLIENT_V2 );

    std::string test_getLogHash( const std::string& param1 ) noexcept( false );
    bool test_setChainParams( const Json::Value& param1 ) noexcept( false );
    bool test_mineBlocks( int param1 ) noexcept( false );
    bool test_modifyTimestamp( int param1 ) noexcept( false );
    bool test_rewindToBlock( int param1 ) noexcept( false );
    std::string web3_sha3( const std::string& param1 ) noexcept( false );
    std::string web3_clientVersion() noexcept( false );
    std::string net_version() noexcept( false );
    std::string net_peerCount() noexcept( false );
    bool net_listening() noexcept( false );
    std::string skale_receiveTransaction( const Json::Value& param1 ) noexcept( false );
    std::string skale_protocolVersion() noexcept( false );
    std::string eth_protocolVersion() noexcept( false );
    std::string skale_shutdownInstance() noexcept( false );
    std::string eth_hashrate() noexcept( false );
    std::string eth_coinbase() noexcept( false );
    bool eth_mining() noexcept( false );
    std::string eth_gasPrice() noexcept( false );
    Json::Value eth_accounts() noexcept( false );
    std::string eth_blockNumber() noexcept( false );
    std::string eth_getBalance( const std::string& param1, const std::string& param2 ) noexcept(
        false );
    std::string eth_getStorageAt( const std::string& param1, const std::string& param2,
        const std::string& param3 ) noexcept( false );
    std::string eth_getTransactionCount(
        const std::string& param1, const std::string& param2 ) noexcept( false );
    Json::Value eth_getBlockTransactionCountByHash( const std::string& param1 ) noexcept( false );
    Json::Value eth_getBlockTransactionCountByNumber( const std::string& param1 ) noexcept( false );
    Json::Value eth_getUncleCountByBlockHash( const std::string& param1 ) noexcept( false );
    Json::Value eth_getUncleCountByBlockNumber( const std::string& param1 ) noexcept( false );
    std::string eth_getCode( const std::string& param1, const std::string& param2 ) noexcept(
        false );
    std::string eth_sendTransaction( const Json::Value& param1 ) noexcept( false );
    std::string eth_call( const Json::Value& param1, const std::string& param2 ) noexcept( false );
    bool eth_flush() noexcept( false );
    Json::Value eth_getBlockByHash( const std::string& param1, bool param2 ) noexcept( false );
    Json::Value eth_getBlockByNumber( const std::string& param1, bool param2 ) noexcept( false );
    Json::Value eth_getTransactionByHash( const std::string& param1 ) noexcept( false );
    Json::Value eth_getTransactionByBlockHashAndIndex(
        const std::string& param1, const std::string& param2 ) noexcept( false );
    Json::Value eth_getTransactionByBlockNumberAndIndex(
        const std::string& param1, const std::string& param2 ) noexcept( false );
    Json::Value eth_getTransactionReceipt( const std::string& param1 ) noexcept( false );
    Json::Value eth_getUncleByBlockHashAndIndex(
        const std::string& param1, const std::string& param2 ) noexcept( false );
    Json::Value eth_getUncleByBlockNumberAndIndex(
        const std::string& param1, const std::string& param2 ) noexcept( false );
    std::string eth_newFilter( const Json::Value& param1 ) noexcept( false );
    std::string eth_newFilterEx( const Json::Value& param1 ) noexcept( false );
    std::string eth_newBlockFilter() noexcept( false );
    std::string eth_newPendingTransactionFilter() noexcept( false );
    bool eth_uninstallFilter( const std::string& param1 ) noexcept( false );
    Json::Value eth_getFilterChanges( const std::string& param1 ) noexcept( false );
    Json::Value eth_getFilterChangesEx( const std::string& param1 ) noexcept( false );
    Json::Value eth_getFilterLogs( const std::string& param1 ) noexcept( false );
    Json::Value eth_getFilterLogsEx( const std::string& param1 ) noexcept( false );
    Json::Value eth_getLogs( const Json::Value& param1 ) noexcept( false );
    Json::Value eth_getLogsEx( const Json::Value& param1 ) noexcept( false );
    Json::Value eth_getWork() noexcept( false );
    bool eth_submitWork( const std::string& param1, const std::string& param2,
        const std::string& param3 ) noexcept( false );
    bool eth_submitHashrate( const std::string& param1, const std::string& param2 ) noexcept(
        false );
    std::string eth_register( const std::string& param1 ) noexcept( false );
    bool eth_unregister( const std::string& param1 ) noexcept( false );
    Json::Value eth_fetchQueuedTransactions( const std::string& param1 ) noexcept( false );
    Json::Value eth_signTransaction( const Json::Value& param1 ) noexcept( false );
    Json::Value eth_subscribe( const Json::Value& param1 ) noexcept( false );
    Json::Value eth_unsubscribe( const Json::Value& param1 ) noexcept( false );
    Json::Value eth_inspectTransaction( const std::string& param1 ) noexcept( false );
    std::string eth_sendRawTransaction( const std::string& param1 ) noexcept( false );
    bool eth_notePassword( const std::string& param1 ) noexcept( false );
    bool db_put( const std::string& param1, const std::string& param2,
        const std::string& param3 ) noexcept( false );
    std::string db_get( const std::string& param1, const std::string& param2 ) noexcept( false );
    bool shh_post( const Json::Value& param1 ) noexcept( false );
    std::string shh_newIdentity() noexcept( false );
    bool shh_hasIdentity( const std::string& param1 ) noexcept( false );
    std::string shh_newGroup( const std::string& param1, const std::string& param2 ) noexcept(
        false );
    std::string shh_addToGroup( const std::string& param1, const std::string& param2 ) noexcept(
        false );
    std::string shh_newFilter( const Json::Value& param1 ) noexcept( false );
    bool shh_uninstallFilter( const std::string& param1 ) noexcept( false );
    Json::Value shh_getFilterChanges( const std::string& param1 ) noexcept( false );
    Json::Value shh_getMessages( const std::string& param1 ) noexcept( false );
    bool admin_web3_setVerbosity( int param1, const std::string& param2 ) noexcept( false );
    bool admin_net_start( const std::string& param1 ) noexcept( false );
    bool admin_net_stop( const std::string& param1 ) noexcept( false );
    bool admin_net_connect( const std::string& param1, const std::string& param2 ) noexcept(
        false );
    Json::Value admin_net_peers( const std::string& param1 ) noexcept( false );
    Json::Value admin_net_nodeInfo( const std::string& param1 ) noexcept( false );
    bool admin_eth_exit( const std::string& param1 ) noexcept( false );
    bool admin_eth_setAskPrice( const std::string& param1, const std::string& param2 ) noexcept(
        false );
    bool admin_eth_setBidPrice( const std::string& param1, const std::string& param2 ) noexcept(
        false );
    bool admin_eth_setReferencePrice(
        const std::string& param1, const std::string& param2 ) noexcept( false );
    bool admin_eth_setPriority( int param1, const std::string& param2 ) noexcept( false );
    bool admin_eth_setMining( bool param1, const std::string& param2 ) noexcept( false );
    Json::Value admin_eth_findBlock(
        const std::string& param1, const std::string& param2 ) noexcept( false );
    Json::Value admin_eth_allAccounts( const std::string& param1 ) noexcept( false );
    Json::Value admin_eth_newAccount(
        const Json::Value& param1, const std::string& param2 ) noexcept( false );
    bool admin_eth_setSigningKey( const std::string& param1, const std::string& param2 ) noexcept(
        false );
    bool admin_eth_setMiningBenefactor(
        const std::string& param1, const std::string& param2 ) noexcept( false );
    Json::Value admin_eth_inspect( const std::string& param1, const std::string& param2 ) noexcept(
        false );
    Json::Value admin_eth_reprocess(
        const std::string& param1, const std::string& param2 ) noexcept( false );
    Json::Value admin_eth_vmTrace(
        const std::string& param1, int param2, const std::string& param3 ) noexcept( false );
    Json::Value admin_eth_getReceiptByHashAndIndex(
        const std::string& param1, int param2, const std::string& param3 ) noexcept( false );
    Json::Value debug_accountRangeAt( const std::string& param1, int param2,
        const std::string& param3, int param4 ) noexcept( false );
    Json::Value debug_traceTransaction(
        const std::string& param1, const Json::Value& param2 ) noexcept( false );
    Json::Value debug_storageRangeAt( const std::string& param1, int param2,
        const std::string& param3, const std::string& param4, int param5 ) noexcept( false );
    std::string debug_preimage( const std::string& param1 ) noexcept( false );
    Json::Value debug_traceBlockByNumber( int param1, const Json::Value& param2 ) noexcept( false );
    Json::Value debug_traceBlockByHash(
        const std::string& param1, const Json::Value& param2 ) noexcept( false );
    Json::Value debug_traceCall( const Json::Value& param1, const std::string& param2,
        const Json::Value& param3 ) noexcept( false );
};

#endif  // JSONRPC_CPP_STUB_WEBTHREESTUBCLIENT_H_
