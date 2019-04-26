/*
    Copyright (C) 2018-present, SKALE Labs

    This file is part of skaled.

    skaled is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skaled is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with skaled.  If not, see <http://www.gnu.org/licenses/>.
*/
/**
 * @file httpserveroverride.cpp
 * @author Dima Litvinov
 * @date 2018
 */

#include "httpserveroverride.h"

#include <libdevcore/microprofile.h>

#include <libdevcore/Common.h>
#include <libdevcore/Log.h>

#include <jsonrpccpp/common/exception.h>
#include <jsonrpccpp/common/specificationparser.h>

#include <cassert>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <vector>

#include <arpa/inet.h>

#include <limits.h>
#include <stdio.h>

#include <libethcore/CommonJS.h>

#if ( defined MSIZE )
#undef MSIZE
#endif

#include <libethereum/Block.h>
#include <libethereum/Transaction.h>
#include <libweb3jsonrpc/JsonHelper.h>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if INT_MAX == 32767
// 16 bits
#define SKALED_WS_SUBSCRIPTION_TYPE_MASK 0xF000
#define SKALED_WS_SUBSCRIPTION_TYPE_NEW_PENDING_TRANSACTION 0x1000
#define SKALED_WS_SUBSCRIPTION_TYPE_NEW_BLOCK 0x2000
#elif INT_MAX == 2147483647
// 32 bits
#define SKALED_WS_SUBSCRIPTION_TYPE_MASK 0xF0000000
#define SKALED_WS_SUBSCRIPTION_TYPE_NEW_PENDING_TRANSACTION 0x10000000
#define SKALED_WS_SUBSCRIPTION_TYPE_NEW_BLOCK 0x20000000
#elif INT_MAX == 9223372036854775807
// 64 bits
#define SKALED_WS_SUBSCRIPTION_TYPE_MASK 0xF000000000000000
#define SKALED_WS_SUBSCRIPTION_TYPE_NEW_PENDING_TRANSACTION 0x1000000000000000
#define SKALED_WS_SUBSCRIPTION_TYPE_NEW_BLOCK 0x2000000000000000
#else
#error "What kind of weird system are you on? We cannot detect size of int"
#endif

namespace skale {
namespace server {
namespace helper {

dev::Verbosity dv_from_ws_msg_type( skutils::ws::e_ws_log_message_type_t eWSLMT ) {
    dev::Verbosity dv = dev::Verbosity::VerbosityTrace;
    switch ( eWSLMT ) {
    case skutils::ws::e_ws_log_message_type_t::eWSLMT_debug:
        dv = dev::Verbosity::VerbosityDebug;
        break;
    case skutils::ws::e_ws_log_message_type_t::eWSLMT_info:
        dv = dev::Verbosity::VerbosityInfo;
        break;
    case skutils::ws::e_ws_log_message_type_t::eWSLMT_warning:
        dv = dev::Verbosity::VerbosityWarning;
        break;
    case skutils::ws::e_ws_log_message_type_t::eWSLMT_error:
        dv = dev::Verbosity::VerbosityWarning;
        break;
    default:
        dv = dev::Verbosity::VerbosityError;
        break;
    }
    return dv;
}

dev::eth::LogFilter toLogFilter( const nlohmann::json& jo ) {
    dev::eth::LogFilter filter;
    if ( ( !jo.is_object() ) || jo.size() == 0 )
        return filter;

    // check only !empty. it should throw exceptions if input params are incorrect
    if ( jo.count( "fromBlock" ) > 0 )
        filter.withEarliest( dev::jsToFixed< 32 >( jo["fromBlock"].get< std::string >() ) );
    if ( jo.count( "toBlock" ) > 0 )
        filter.withLatest( dev::jsToFixed< 32 >( jo["toBlock"].get< std::string >() ) );
    if ( jo.count( "address" ) > 0 ) {
        if ( jo["address"].is_array() )
            for ( auto i : jo["address"] )
                filter.address( dev::jsToAddress( i.get< std::string >() ) );
        else
            filter.address( dev::jsToAddress( jo["address"].get< std::string >() ) );
    }
    if ( jo.count( "topics" ) > 0 )
        for ( unsigned i = 0; i < jo["topics"].size(); i++ ) {
            if ( jo["topics"][i].is_array() ) {
                for ( auto t : jo["topics"][i] )
                    if ( !t.is_null() )
                        filter.topic( i, dev::jsToFixed< 32 >( t.get< std::string >() ) );
            } else if ( !jo["topics"][i].is_null() )  // if it is anything else then string, it
                                                      // should and will fail
                filter.topic( i, dev::jsToFixed< 32 >( jo["topics"][i].get< std::string >() ) );
        }
    return filter;
}

dev::eth::LogFilter toLogFilter( const nlohmann::json& jo, dev::eth::Interface const& _client ) {
    dev::eth::LogFilter filter;
    if ( ( !jo.is_object() ) || jo.size() == 0 )
        return filter;
    // check only !empty. it should throw exceptions if input params are incorrect
    if ( jo.count( "fromBlock" ) > 0 )
        filter.withEarliest( _client.hashFromNumber(
            dev::eth::jsToBlockNumber( jo["fromBlock"].get< std::string >() ) ) );
    if ( jo.count( "toBlock" ) > 0 )
        filter.withLatest( _client.hashFromNumber(
            dev::eth::jsToBlockNumber( jo["toBlock"].get< std::string >() ) ) );
    if ( jo.count( "address" ) > 0 ) {
        if ( jo.count( "address" ) > 0 )
            for ( auto i : jo["address"] )
                filter.address( dev::jsToAddress( i.get< std::string >() ) );
        else
            filter.address( dev::jsToAddress( jo["address"].get< std::string >() ) );
    }
    if ( jo.count( "topics" ) > 0 )
        for ( unsigned i = 0; i < jo["topics"].size(); i++ ) {
            if ( jo["topics"][i].is_array() ) {
                for ( auto t : jo["topics"][i] )
                    if ( !t.is_null() )
                        filter.topic( i, dev::jsToFixed< 32 >( t.get< std::string >() ) );
            } else if ( !jo["topics"][i].is_null() )  // if it is anything else then string, it
                                                      // should and will fail
                filter.topic( i, dev::jsToFixed< 32 >( jo["topics"][i].get< std::string >() ) );
        }
    return filter;
}

nlohmann::json nljsBlockNumber( dev::eth::BlockNumber bn ) {
    if ( bn == dev::eth::LatestBlock )
        return nlohmann::json( "latest" );
    if ( bn == 0 )
        return nlohmann::json( "earliest" );
    if ( bn == dev::eth::PendingBlock )
        return nlohmann::json( "pending" );
    return nlohmann::json( unsigned( bn ) );
}

nlohmann::json toJson( std::unordered_map< dev::h256, dev::eth::LocalisedLogEntries > const& eb,
    std::vector< dev::h256 > const& order ) {
    nlohmann::json res = nlohmann::json::array();
    for ( auto const& i : order ) {
        auto entries = eb.at( i );
        nlohmann::json currentBlock = nlohmann::json::object();
        dev::eth::LocalisedLogEntry entry = entries[0];
        if ( entry.mined ) {
            currentBlock["blockNumber"] = nljsBlockNumber( entry.blockNumber );
            currentBlock["blockHash"] = toJS( entry.blockHash );
            currentBlock["type"] = "mined";
        } else
            currentBlock["type"] = "pending";
        currentBlock["polarity"] = entry.polarity == dev::eth::BlockPolarity::Live ? true : false;
        currentBlock["logs"] = nlohmann::json::array();
        for ( dev::eth::LocalisedLogEntry const& e : entries ) {
            nlohmann::json log = nlohmann::json::object();
            log["logIndex"] = e.logIndex;
            log["transactionIndex"] = e.transactionIndex;
            log["transactionHash"] = toJS( e.transactionHash );
            log["address"] = dev::toJS( e.address );
            log["data"] = dev::toJS( e.data );
            log["topics"] = nlohmann::json::array();
            for ( auto const& t : e.topics )
                log["topics"].push_back( dev::toJS( t ) );
            currentBlock["logs"].push_back( log );
        }
        res.push_back( currentBlock );
    }
    return res;
}

nlohmann::json toJsonByBlock( dev::eth::LocalisedLogEntries const& le ) {
    std::vector< dev::h256 > order;
    std::unordered_map< dev::h256, dev::eth::LocalisedLogEntries > entriesByBlock;
    for ( dev::eth::LocalisedLogEntry const& e : le ) {
        if ( e.isSpecial )  // skip special log
            continue;
        if ( entriesByBlock.count( e.blockHash ) == 0 ) {
            entriesByBlock[e.blockHash] = dev::eth::LocalisedLogEntries();
            order.push_back( e.blockHash );
        }
        entriesByBlock[e.blockHash].push_back( e );
    }
    return toJson( entriesByBlock, order );
}

};  // namespace helper
};  // namespace server
};  // namespace skale


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

SkaleWsPeer::SkaleWsPeer( skutils::ws::server& srv, const skutils::ws::hdl_t& hdl )
    : skutils::ws::peer( srv, hdl ),
      strPeerQueueID_( skutils::dispatch::generate_id( this, "relay_peer" ) ) {
    if ( pso()->bTraceCalls_ )
        clog( dev::VerbosityTrace, cc::info( getRelay().scheme_uc_ ) )
            << desc() << cc::notice( " peer ctor" );
}
SkaleWsPeer::~SkaleWsPeer() {
    if ( pso()->bTraceCalls_ )
        clog( dev::VerbosityTrace, cc::info( getRelay().scheme_uc_ ) )
            << desc() << cc::notice( " peer dctor" );
    uninstallAllWatches();
    skutils::dispatch::remove( strPeerQueueID_ );
}

void SkaleWsPeer::onPeerRegister() {
    if ( pso()->bTraceCalls_ )
        clog( dev::VerbosityInfo, cc::info( getRelay().scheme_uc_ ) )
            << desc() << cc::notice( " peer registered" );
    skutils::ws::peer::onPeerRegister();
}
void SkaleWsPeer::onPeerUnregister() {  // peer will no longer receive onMessage after call to
                                        // this
    if ( pso()->bTraceCalls_ )
        clog( dev::VerbosityInfo, cc::info( getRelay().scheme_uc_ ) )
            << desc() << cc::notice( " peer unregistered" );
    skutils::ws::peer::onPeerUnregister();
    uninstallAllWatches();
}

void SkaleWsPeer::onMessage( const std::string& msg, skutils::ws::opcv eOpCode ) {
    if ( eOpCode != skutils::ws::opcv::text )
        throw std::runtime_error( "only ws text messages are supported" );
    skutils::dispatch::async( strPeerQueueID_, [=]() -> void {
        std::string strRequest( msg );
        if ( pso()->bTraceCalls_ )
            clog( dev::VerbosityInfo, cc::info( getRelay().scheme_uc_ ) )
                << cc::ws_rx_inv( " >>> " + getRelay().scheme_uc_ + "/RX >>> " ) << desc()
                << cc::ws_rx( " >>> " ) << cc::j( strRequest );
        this->ref_retain();  // manual ref management
        skutils::dispatch::async( strPeerQueueID_, [this, strRequest]() -> void {
            try {
                int nID = -1;
                std::string strResponse;
                try {
                    nlohmann::json joRequest = nlohmann::json::parse( strRequest );
                    if ( !handleWebSocketSpecificRequest( joRequest, strResponse ) ) {
                        nID = joRequest["id"].get< int >();
                        jsonrpc::IClientConnectionHandler* handler = pso()->GetHandler( "/" );
                        if ( handler == nullptr )
                            throw std::runtime_error( "No client connection handler found" );
                        handler->HandleRequest( strRequest, strResponse );
                    }
                } catch ( const std::exception& ex ) {
                    clog( dev::VerbosityError, cc::info( getRelay().scheme_uc_ ) )
                        << cc::ws_tx_inv( " !!! " + getRelay().scheme_uc_ + "/ERR !!! " ) << desc()
                        << cc::ws_tx( " !!! " ) << cc::warn( ex.what() );
                    nlohmann::json joErrorResponce;
                    joErrorResponce["id"] = nID;
                    joErrorResponce["result"] = "error";
                    joErrorResponce["error"] = std::string( ex.what() );
                    strResponse = joErrorResponce.dump();
                } catch ( ... ) {
                    const char* e = "unknown exception in SkaleServerOverride";
                    clog( dev::VerbosityError, cc::info( getRelay().scheme_uc_ ) )
                        << cc::ws_tx_inv( " !!! " + getRelay().scheme_uc_ + "/ERR !!! " ) << desc()
                        << cc::ws_tx( " !!! " ) << cc::warn( e );
                    nlohmann::json joErrorResponce;
                    joErrorResponce["id"] = nID;
                    joErrorResponce["result"] = "error";
                    joErrorResponce["error"] = std::string( e );
                    strResponse = joErrorResponce.dump();
                }
                if ( pso()->bTraceCalls_ )
                    clog( dev::VerbosityInfo, cc::info( getRelay().scheme_uc_ ) )
                        << cc::ws_tx_inv( " <<< " + getRelay().scheme_uc_ + "/TX <<< " ) << desc()
                        << cc::ws_tx( " <<< " ) << cc::j( strResponse );
                sendMessage( strResponse );
            } catch ( ... ) {
            }
            this->ref_release();  // manual ref management
        } );
    } );
    // skutils::ws::peer::onMessage( msg, eOpCode );
}

void SkaleWsPeer::onClose(
    const std::string& reason, int local_close_code, const std::string& local_close_code_as_str ) {
    if ( pso()->bTraceCalls_ )
        clog( dev::VerbosityInfo, cc::info( getRelay().scheme_uc_ ) )
            << desc() << cc::warn( " peer close event with code=" ) << cc::c( local_close_code )
            << cc::debug( ", reason=" ) << cc::info( reason ) << "\n";
    skutils::ws::peer::onClose( reason, local_close_code, local_close_code_as_str );
    uninstallAllWatches();
}

void SkaleWsPeer::onFail() {
    if ( pso()->bTraceCalls_ )
        clog( dev::VerbosityError, cc::fatal( getRelay().scheme_uc_ ) )
            << desc() << cc::error( " peer fail event" ) << "\n";
    skutils::ws::peer::onFail();
    uninstallAllWatches();
}

void SkaleWsPeer::onLogMessage(
    skutils::ws::e_ws_log_message_type_t eWSLMT, const std::string& msg ) {
    if ( pso()->bTraceCalls_ )
        clog( skale::server::helper::dv_from_ws_msg_type( eWSLMT ),
            cc::info( getRelay().scheme_uc_ ) )
            << desc() << cc::debug( " peer log: " ) << msg << "\n";
    skutils::ws::peer::onLogMessage( eWSLMT, msg );
}

SkaleWsRelay& SkaleWsPeer::getRelay() {
    return static_cast< SkaleWsRelay& >( srv() );
}
SkaleServerOverride* SkaleWsPeer::pso() {
    return getRelay().pso();
}
dev::eth::Interface* SkaleWsPeer::ethereum() const {
    return pso()->ethereum();
}

void SkaleWsPeer::uninstallAllWatches() {
    set_watche_ids_t sw;
    //
    sw = setInstalledWatchesLogs_;
    setInstalledWatchesLogs_.clear();
    for ( auto iw : sw ) {
        try {
            ethereum()->uninstallWatch( iw );
        } catch ( ... ) {
        }
    }
    //
    sw = setInstalledWatchesNewPendingTransactions_;
    setInstalledWatchesNewPendingTransactions_.clear();
    for ( auto iw : sw ) {
        try {
            ethereum()->uninstallNewPendingTransactionWatch( iw );
        } catch ( ... ) {
        }
    }
    //
    sw = setInstalledWatchesNewBlocks_;
    setInstalledWatchesNewBlocks_.clear();
    for ( auto iw : sw ) {
        try {
            ethereum()->uninstallNewBlockWatch( iw );
        } catch ( ... ) {
        }
    }
}

bool SkaleWsPeer::handleWebSocketSpecificRequest(
    const nlohmann::json& joRequest, std::string& strResponse ) {
    strResponse.clear();
    nlohmann::json joResponse = nlohmann::json::object();
    joResponse["jsonrpc"] = "2.0";
    joResponse["id"] = joRequest["id"];
    joResponse["result"] = nullptr;
    if ( !handleWebSocketSpecificRequest( joRequest, joResponse ) )
        return false;
    strResponse = joResponse.dump();
    return true;
}

bool SkaleWsPeer::handleWebSocketSpecificRequest(
    const nlohmann::json& joRequest, nlohmann::json& joResponse ) {
    std::string strMethod = joRequest["method"].get< std::string >();
    rpc_map_t::const_iterator itFind = g_rpc_map.find( strMethod );
    if ( itFind == g_rpc_map.end() )
        return false;
    ( ( *this ).*( itFind->second ) )( joRequest, joResponse );
    return true;
}

const SkaleWsPeer::rpc_map_t SkaleWsPeer::g_rpc_map = {
    {"eth_subscribe", &SkaleWsPeer::eth_subscribe},
    {"eth_unsubscribe", &SkaleWsPeer::eth_unsubscribe},
};

bool SkaleWsPeer::checkParamsPresent(
    const char* strMethodName, const nlohmann::json& joRequest, nlohmann::json& joResponse ) {
    if ( joRequest.count( "params" ) > 0 )
        return true;
    if ( pso()->bTraceCalls_ )
        clog( dev::Verbosity::VerbosityError, cc::info( getRelay().scheme_uc_ ) )
            << desc() << " " << cc::error( "error in " ) << cc::warn( strMethodName )
            << cc::error( " rpc method, json entry " ) << cc::warn( "params" )
            << cc::error( " is missing" ) << "\n";
    nlohmann::json joError = nlohmann::json::object();
    joError["code"] = -32602;
    joError["message"] = std::string( "error in \"" ) + strMethodName +
                         "\" rpc method, json entry \"params\" is missing";
    joResponse["error"] = joError;
    return false;
}

bool SkaleWsPeer::checkParamsIsArray(
    const char* strMethodName, const nlohmann::json& joRequest, nlohmann::json& joResponse ) {
    if ( !checkParamsPresent( strMethodName, joRequest, joResponse ) )
        return false;
    const nlohmann::json& jarrParams = joRequest["params"];
    if ( jarrParams.is_array() )
        return true;
    if ( pso()->bTraceCalls_ )
        clog( dev::Verbosity::VerbosityError, cc::info( getRelay().scheme_uc_ ) )
            << desc() << " " << cc::error( "error in " ) << cc::warn( strMethodName )
            << cc::error( " rpc method, json entry " ) << cc::warn( "params" )
            << cc::error( " must be array" ) << "\n";
    nlohmann::json joError = nlohmann::json::object();
    joError["code"] = -32602;
    joError["message"] = std::string( "error in \"" ) + strMethodName +
                         "\" rpc method, json entry \"params\" must be array";
    joResponse["error"] = joError;
    return false;
}

void SkaleWsPeer::eth_subscribe( const nlohmann::json& joRequest, nlohmann::json& joResponse ) {
    if ( !checkParamsIsArray( "eth_subscribe", joRequest, joResponse ) )
        return;
    const nlohmann::json& jarrParams = joRequest["params"];
    std::string strSubcscriptionType;
    size_t idxParam, cntParams = jarrParams.size();
    for ( idxParam = 0; idxParam < cntParams; ++idxParam ) {
        const nlohmann::json& joParamItem = jarrParams[idxParam];
        if ( !joParamItem.is_string() )
            continue;
        strSubcscriptionType = skutils::tools::trim_copy( joParamItem.get< std::string >() );
        break;
    }
    if ( strSubcscriptionType == "logs" ) {
        eth_subscribe_logs( joRequest, joResponse );
        return;
    }
    if ( strSubcscriptionType == "newPendingTransactions" ||
         strSubcscriptionType == "pendingTransactions" ) {
        eth_subscribe_newPendingTransactions( joRequest, joResponse );
        return;
    }
    if ( strSubcscriptionType == "newHeads" || strSubcscriptionType == "newBlockHeaders" ) {
        eth_subscribe_newHeads( joRequest, joResponse );
        return;
    }
    if ( strSubcscriptionType.empty() )
        strSubcscriptionType = "<empty>";
    if ( pso()->bTraceCalls_ )
        clog( dev::Verbosity::VerbosityError, cc::info( getRelay().scheme_uc_ ) )
            << desc() << " " << cc::error( "error in " ) << cc::warn( "eth_subscribe" )
            << cc::error(
                   " rpc method, missing valid subscription type in parameters, was specifiedL " )
            << cc::warn( strSubcscriptionType ) << "\n";
    nlohmann::json joError = nlohmann::json::object();
    joError["code"] = -32603;
    joError["message"] =
        "error in \"eth_subscribe\" rpc method, missing valid subscription type in parameters, was "
        "specified: " +
        strSubcscriptionType;
    joResponse["error"] = joError;
}

void SkaleWsPeer::eth_subscribe_logs(
    const nlohmann::json& joRequest, nlohmann::json& joResponse ) {
    try {
        const nlohmann::json& jarrParams = joRequest["params"];
        dev::eth::LogFilter logFilter;
        bool bHaveLogFilter = false;
        size_t idxParam, cntParams = jarrParams.size();
        for ( idxParam = 0; idxParam < cntParams; ++idxParam ) {
            const nlohmann::json& joParamItem = jarrParams[idxParam];
            if ( joParamItem.is_string() )
                continue;
            if ( joParamItem.is_object() ) {
                if ( !bHaveLogFilter ) {
                    bHaveLogFilter = true;
                    logFilter = skale::server::helper::toLogFilter( joParamItem, *ethereum() );
                }
            }
        }  // for ( idxParam = 0; idxParam < cntParams; ++idxParam )
        skutils::retain_release_ptr< SkaleWsPeer > pThis( this );
        dev::eth::fnClientWatchHandlerMulti_t fnOnSunscriptionEvent;
        fnOnSunscriptionEvent += [pThis]( unsigned iw ) -> void {
            skutils::dispatch::async( pThis->strPeerQueueID_, [pThis, iw]() -> void {
                dev::eth::LocalisedLogEntries le = pThis->ethereum()->logs( iw );
                nlohmann::json joResult = skale::server::helper::toJsonByBlock( le );
                if ( joResult.is_array() ) {
                    for ( const auto& joRW : joResult ) {
                        if ( joRW.is_object() && joRW.count( "logs" ) > 0 &&
                             joRW.count( "blockHash" ) > 0 && joRW.count( "blockNumber" ) > 0 ) {
                            std::string strBlockHash = joRW["blockHash"].get< std::string >();
                            unsigned nBlockBumber = joRW["blockNumber"].get< unsigned >();
                            const nlohmann::json& joResultLogs = joRW["logs"];
                            if ( joResultLogs.is_array() ) {
                                for ( const auto& joWalk : joResultLogs ) {
                                    if ( !joWalk.is_object() )
                                        continue;
                                    nlohmann::json joLog = joWalk;  // copy
                                    joLog["blockHash"] = strBlockHash;
                                    joLog["blockNumber"] = nBlockBumber;
                                    nlohmann::json joParams = nlohmann::json::object();
                                    joParams["susbcription"] = dev::toJS( iw );
                                    joParams["result"] = joLog;
                                    nlohmann::json joNotification = nlohmann::json::object();
                                    joNotification["jsonrpc"] = "2.0";
                                    joNotification["method"] = "eth_subscription";
                                    joNotification["params"] = joParams;
                                    std::string strNotification = joNotification.dump();
                                    if ( pThis->pso()->bTraceCalls_ )
                                        clog( dev::VerbosityInfo,
                                            cc::info( pThis->getRelay().scheme_uc_ ) )
                                            << cc::ws_tx_inv( " <<< " +
                                                              pThis->getRelay().scheme_uc_ +
                                                              "/TX <<< " )
                                            << pThis->desc() << cc::ws_tx( " <<< " )
                                            << cc::j( strNotification );
                                    skutils::dispatch::async(
                                        pThis->strPeerQueueID_, [pThis, strNotification]() -> void {
                                            const_cast< SkaleWsPeer* >( pThis.get() )
                                                ->sendMessage( strNotification );
                                        } );
                                }  // for ( const auto& joWalk : joResultLogs )
                            }      // if ( joResultLogs.is_array() )
                        }
                    }  // for ( const auto& joRW : joResult )
                }      // if ( joResult.is_array() )
            } );
        };
        unsigned iw = ethereum()->installWatch(
            logFilter, dev::eth::Reaping::Automatic, fnOnSunscriptionEvent );
        setInstalledWatchesLogs_.insert( iw );
        std::string strIW = dev::toJS( iw );
        if ( pso()->bTraceCalls_ )
            clog( dev::Verbosity::VerbosityTrace, cc::info( getRelay().scheme_uc_ ) )
                << desc() << " " << cc::info( "eth_subscribe/logs" )
                << cc::debug( " rpc method did installed watch " ) << cc::info( strIW ) << "\n";
        joResponse["result"] = strIW;
    } catch ( const std::exception& ex ) {
        if ( pso()->bTraceCalls_ )
            clog( dev::Verbosity::VerbosityError, cc::info( getRelay().scheme_uc_ ) )
                << desc() << " " << cc::error( "error in " ) << cc::warn( "eth_subscribe/logs" )
                << cc::error( " rpc method, exception " ) << cc::warn( ex.what() ) << "\n";
        nlohmann::json joError = nlohmann::json::object();
        joError["code"] = -32602;
        joError["message"] =
            std::string( "error in \"eth_subscribe/logs\" rpc method, exception: " ) + ex.what();
        joResponse["error"] = joError;
        return;
    } catch ( ... ) {
        if ( pso()->bTraceCalls_ )
            clog( dev::Verbosity::VerbosityError, cc::info( getRelay().scheme_uc_ ) )
                << desc() << " " << cc::error( "error in " ) << cc::warn( "eth_subscribe/logs" )
                << cc::error( " rpc method, unknown exception " ) << "\n";
        nlohmann::json joError = nlohmann::json::object();
        joError["code"] = -32602;
        joError["message"] = "error in \"eth_subscribe/logs\" rpc method, unknown exception";
        joResponse["error"] = joError;
        return;
    }
}

void SkaleWsPeer::eth_subscribe_newPendingTransactions(
    const nlohmann::json& joRequest, nlohmann::json& joResponse ) {
    try {
        skutils::retain_release_ptr< SkaleWsPeer > pThis( this );
        std::function< void( const unsigned& iw, const dev::eth::Transaction& t ) >
            fnOnSunscriptionEvent =
                [pThis]( const unsigned& iw, const dev::eth::Transaction& t ) -> void {


        };
        unsigned iw = ethereum()->installNewPendingTransactionWatch( fnOnSunscriptionEvent );
        setInstalledWatchesNewPendingTransactions_.insert( iw );
        iw |= SKALED_WS_SUBSCRIPTION_TYPE_NEW_PENDING_TRANSACTION;
        std::string strIW = dev::toJS( iw );
        if ( pso()->bTraceCalls_ )
            clog( dev::Verbosity::VerbosityTrace, cc::info( getRelay().scheme_uc_ ) )
                << desc() << " " << cc::info( "eth_subscribe/newPendingTransactions" )
                << cc::debug( " rpc method did installed watch " ) << cc::info( strIW ) << "\n";
        joResponse["result"] = strIW;
    } catch ( const std::exception& ex ) {
        if ( pso()->bTraceCalls_ )
            clog( dev::Verbosity::VerbosityError, cc::info( getRelay().scheme_uc_ ) )
                << desc() << " " << cc::error( "error in " )
                << cc::warn( "eth_subscribe/newPendingTransactions" )
                << cc::error( " rpc method, exception " ) << cc::warn( ex.what() ) << "\n";
        nlohmann::json joError = nlohmann::json::object();
        joError["code"] = -32602;
        joError["message"] =
            std::string(
                "error in \"eth_subscribe/newPendingTransactions\" rpc method, exception: " ) +
            ex.what();
        joResponse["error"] = joError;
        return;
    } catch ( ... ) {
        if ( pso()->bTraceCalls_ )
            clog( dev::Verbosity::VerbosityError, cc::info( getRelay().scheme_uc_ ) )
                << desc() << " " << cc::error( "error in " )
                << cc::warn( "eth_subscribe/newPendingTransactions" )
                << cc::error( " rpc method, unknown exception " ) << "\n";
        nlohmann::json joError = nlohmann::json::object();
        joError["code"] = -32602;
        joError["message"] =
            "error in \"eth_subscribe/newPendingTransactions\" rpc method, unknown exception";
        joResponse["error"] = joError;
        return;
    }
}

void SkaleWsPeer::eth_subscribe_newHeads(
    const nlohmann::json& joRequest, nlohmann::json& joResponse, bool bIncludeTransactions ) {
    try {
        skutils::retain_release_ptr< SkaleWsPeer > pThis( this );
        std::function< void( const unsigned& iw, const dev::eth::Block& block ) >
            fnOnSunscriptionEvent = [pThis, bIncludeTransactions](
                                        const unsigned& iw, const dev::eth::Block& block ) -> void {
            dev::h256 h = block.info().hash();
            Json::Value jv;
            if ( bIncludeTransactions )
                jv = dev::eth::toJson( pThis->ethereum()->blockInfo( h ),
                    pThis->ethereum()->blockDetails( h ), pThis->ethereum()->uncleHashes( h ),
                    pThis->ethereum()->transactions( h ), pThis->ethereum()->sealEngine() );
            else
                jv = dev::eth::toJson( pThis->ethereum()->blockInfo( h ),
                    pThis->ethereum()->blockDetails( h ), pThis->ethereum()->uncleHashes( h ),
                    pThis->ethereum()->transactionHashes( h ), pThis->ethereum()->sealEngine() );
            Json::FastWriter fastWriter;
            std::string s = fastWriter.write( jv );
            nlohmann::json joBlockDescription = nlohmann::json::parse( s );
            //
            nlohmann::json joParams = nlohmann::json::object();
            joParams["susbcription"] = dev::toJS( iw | SKALED_WS_SUBSCRIPTION_TYPE_NEW_BLOCK );
            joParams["result"] = joBlockDescription;
            nlohmann::json joNotification = nlohmann::json::object();
            joNotification["jsonrpc"] = "2.0";
            joNotification["method"] = "eth_subscription";
            joNotification["params"] = joParams;
            std::string strNotification = joNotification.dump();
            if ( pThis->pso()->bTraceCalls_ )
                clog( dev::VerbosityInfo, cc::info( pThis->getRelay().scheme_uc_ ) )
                    << cc::ws_tx_inv( " <<< " + pThis->getRelay().scheme_uc_ + "/TX <<< " )
                    << pThis->desc() << cc::ws_tx( " <<< " ) << cc::j( strNotification );
            skutils::dispatch::async( pThis->strPeerQueueID_, [pThis, strNotification]() -> void {
                const_cast< SkaleWsPeer* >( pThis.get() )->sendMessage( strNotification );
            } );
        };
        unsigned iw = ethereum()->installNewBlockWatch( fnOnSunscriptionEvent );
        setInstalledWatchesNewBlocks_.insert( iw );
        iw |= SKALED_WS_SUBSCRIPTION_TYPE_NEW_BLOCK;
        std::string strIW = dev::toJS( iw );
        if ( pso()->bTraceCalls_ )
            clog( dev::Verbosity::VerbosityTrace, cc::info( getRelay().scheme_uc_ ) )
                << desc() << " " << cc::info( "eth_subscribe/newHeads" )
                << cc::debug( " rpc method did installed watch " ) << cc::info( strIW ) << "\n";
        joResponse["result"] = strIW;
    } catch ( const std::exception& ex ) {
        if ( pso()->bTraceCalls_ )
            clog( dev::Verbosity::VerbosityError, cc::info( getRelay().scheme_uc_ ) )
                << desc() << " " << cc::error( "error in " )
                << cc::warn( "eth_subscribe/newHeads(" ) << cc::error( " rpc method, exception " )
                << cc::warn( ex.what() ) << "\n";
        nlohmann::json joError = nlohmann::json::object();
        joError["code"] = -32602;
        joError["message"] =
            std::string( "error in \"eth_subscribe/newHeads(\" rpc method, exception: " ) +
            ex.what();
        joResponse["error"] = joError;
        return;
    } catch ( ... ) {
        if ( pso()->bTraceCalls_ )
            clog( dev::Verbosity::VerbosityError, cc::info( getRelay().scheme_uc_ ) )
                << desc() << " " << cc::error( "error in " )
                << cc::warn( "eth_subscribe/newHeads(" )
                << cc::error( " rpc method, unknown exception " ) << "\n";
        nlohmann::json joError = nlohmann::json::object();
        joError["code"] = -32602;
        joError["message"] = "error in \"eth_subscribe/newHeads(\" rpc method, unknown exception";
        joResponse["error"] = joError;
        return;
    }
}


void SkaleWsPeer::eth_unsubscribe( const nlohmann::json& joRequest, nlohmann::json& joResponse ) {
    if ( !checkParamsIsArray( "eth_unsubscribe", joRequest, joResponse ) )
        return;
    const nlohmann::json& jarrParams = joRequest["params"];
    size_t idxParam, cntParams = jarrParams.size();
    for ( idxParam = 0; idxParam < cntParams; ++idxParam ) {
        const nlohmann::json& joParamItem = jarrParams[idxParam];
        unsigned iw = unsigned( -1 );
        if ( joParamItem.is_string() ) {
            std::string strIW = skutils::tools::trim_copy( joParamItem.get< std::string >() );
            if ( !strIW.empty() )
                iw = unsigned( std::stoul( strIW.c_str(), nullptr, 0 ) );
        } else if ( joParamItem.is_number_integer() ) {
            iw = joParamItem.get< unsigned >();
        }
        if ( iw == unsigned( -1 ) ) {
            if ( pso()->bTraceCalls_ )
                clog( dev::Verbosity::VerbosityError, cc::info( getRelay().scheme_uc_ ) )
                    << desc() << " " << cc::error( "error in " ) << cc::warn( "eth_unsubscribe" )
                    << cc::error( " rpc method, bad subsription ID " ) << cc::j( joParamItem )
                    << "\n";
            nlohmann::json joError = nlohmann::json::object();
            joError["code"] = -32602;
            joError["message"] =
                "error in \"eth_unsubscribe\" rpc method, ad subsription ID " + joParamItem.dump();
            joResponse["error"] = joError;
            return;
        }
        unsigned x = ( iw & SKALED_WS_SUBSCRIPTION_TYPE_MASK );
        if ( x == SKALED_WS_SUBSCRIPTION_TYPE_NEW_PENDING_TRANSACTION ) {
            if ( setInstalledWatchesNewPendingTransactions_.find(
                     iw & ( !( SKALED_WS_SUBSCRIPTION_TYPE_MASK ) ) ) ==
                 setInstalledWatchesNewPendingTransactions_.end() ) {
                std::string strIW = dev::toJS( iw );
                if ( pso()->bTraceCalls_ )
                    clog( dev::Verbosity::VerbosityError, cc::info( getRelay().scheme_uc_ ) )
                        << desc() << " " << cc::error( "error in " )
                        << cc::warn( "eth_unsubscribe/newPendingTransactionWatch" )
                        << cc::error( " rpc method, bad subsription ID " ) << cc::warn( strIW )
                        << "\n";
                nlohmann::json joError = nlohmann::json::object();
                joError["code"] = -32602;
                joError["message"] =
                    "error in \"eth_unsubscribe/newPendingTransactionWatch\" rpc method, ad "
                    "subsription ID " +
                    strIW;
                joResponse["error"] = joError;
                return;
            }
            ethereum()->uninstallNewPendingTransactionWatch( iw );
            setInstalledWatchesNewPendingTransactions_.erase(
                iw & ( !( SKALED_WS_SUBSCRIPTION_TYPE_MASK ) ) );
        } else if ( x == SKALED_WS_SUBSCRIPTION_TYPE_NEW_BLOCK ) {
            if ( setInstalledWatchesNewBlocks_.find(
                     iw & ( !( SKALED_WS_SUBSCRIPTION_TYPE_MASK ) ) ) ==
                 setInstalledWatchesNewBlocks_.end() ) {
                std::string strIW = dev::toJS( iw );
                if ( pso()->bTraceCalls_ )
                    clog( dev::Verbosity::VerbosityError, cc::info( getRelay().scheme_uc_ ) )
                        << desc() << " " << cc::error( "error in " )
                        << cc::warn( "eth_unsubscribe/newHeads" )
                        << cc::error( " rpc method, bad subsription ID " ) << cc::warn( strIW )
                        << "\n";
                nlohmann::json joError = nlohmann::json::object();
                joError["code"] = -32602;
                joError["message"] =
                    "error in \"eth_unsubscribe/newHeads\" rpc method, ad subsription ID " + strIW;
                joResponse["error"] = joError;
                return;
            }
            ethereum()->uninstallNewBlockWatch( iw );
            setInstalledWatchesNewBlocks_.erase( iw & ( !( SKALED_WS_SUBSCRIPTION_TYPE_MASK ) ) );
        } else {
            if ( setInstalledWatchesLogs_.find( iw ) == setInstalledWatchesLogs_.end() ) {
                std::string strIW = dev::toJS( iw );
                if ( pso()->bTraceCalls_ )
                    clog( dev::Verbosity::VerbosityError, cc::info( getRelay().scheme_uc_ ) )
                        << desc() << " " << cc::error( "error in " )
                        << cc::warn( "eth_unsubscribe/logs" )
                        << cc::error( " rpc method, bad subsription ID " ) << cc::warn( strIW )
                        << "\n";
                nlohmann::json joError = nlohmann::json::object();
                joError["code"] = -32602;
                joError["message"] =
                    "error in \"eth_unsubscribe/logs\" rpc method, ad subsription ID " + strIW;
                joResponse["error"] = joError;
                return;
            }
            ethereum()->uninstallWatch( iw );
            setInstalledWatchesLogs_.erase( iw );
        }
    }  // for ( idxParam = 0; idxParam < cntParams; ++idxParam )
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

SkaleWsRelay::SkaleWsRelay( const char* strScheme,  // "ws" or "wss"
    int nPort )
    : scheme_( skutils::tools::to_lower( strScheme ) ),
      scheme_uc_( skutils::tools::to_upper( strScheme ) ),
      nPort_( nPort ) {
    onPeerInstantiate_ = [&]( skutils::ws::server& srv,
                             skutils::ws::hdl_t hdl ) -> skutils::ws::peer_ptr_t {
        if ( pso()->bTraceCalls_ )
            clog( dev::VerbosityTrace, cc::info( scheme_uc_ ) )
                << cc::notice( "Will instantiate new peer" );
        SkaleWsPeer* pSkalePeer = new SkaleWsPeer( srv, hdl );
        return pSkalePeer;
    };
    onPeerRegister_ = [&]( skutils::ws::peer_ptr_t& pPeer ) -> void {
        SkaleWsPeer* pSkalePeer = dynamic_cast< SkaleWsPeer* >( pPeer );
        if ( pSkalePeer == nullptr ) {
            std::cerr << "SLAKLE server fatal error: bad WS peer interface(1)\n";
            std::cerr.flush();
            std::terminate();
        }
        lock_type lock( mtxAllPeers() );
        mapAllPeers_[pSkalePeer->strPeerQueueID_] = pSkalePeer;
    };
    onPeerUnregister_ = [&]( skutils::ws::peer_ptr_t& pPeer ) -> void {
        SkaleWsPeer* pSkalePeer = dynamic_cast< SkaleWsPeer* >( pPeer );
        if ( pSkalePeer == nullptr ) {
            std::cerr << "SLAKLE server fatal error: bad WS peer interface(2)\n";
            std::cerr.flush();
            std::terminate();
        }
        lock_type lock( mtxAllPeers() );
        mapAllPeers_.erase( pSkalePeer->strPeerQueueID_ );
    };
}

SkaleWsRelay::~SkaleWsRelay() {
    stop();
}

void SkaleWsRelay::run( skutils::ws::fn_continue_status_flag_t fnContinueStatusFlag ) {
    isInLoop_ = true;
    try {
        if ( service_mode_supported() )
            service( fnContinueStatusFlag );
        else {
            while ( true ) {
                poll( fnContinueStatusFlag );
                if ( fnContinueStatusFlag ) {
                    if ( !fnContinueStatusFlag() )
                        break;
                }
            }
        }
    } catch ( ... ) {
    }
    isInLoop_ = false;
}

void SkaleWsRelay::waitWhileInLoop() {
    while ( isInLoop() )
        std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
}

bool SkaleWsRelay::start( SkaleServerOverride* pso ) {
    stop();
    pso_ = pso;
    clog( dev::VerbosityInfo, cc::info( scheme_uc_ ) )
        << cc::notice( "Will start server on port " ) << cc::c( nPort_ );
    if ( !open( scheme_, nPort_ ) ) {
        clog( dev::VerbosityError, cc::fatal( scheme_uc_ + " ERRORL" ) )
            << cc::error( "Failed to start serv on port " ) << cc::c( nPort_ );
        return false;
    }
    std::thread( [&]() {
        isRunning_ = true;
        try {
            run( [&]() -> bool { return isRunning_; } );
        } catch ( ... ) {
        }
        // isRunning_ = false;
    } )
        .detach();
    clog( dev::VerbosityInfo, cc::info( scheme_uc_ ) )
        << cc::success( "OK, server started on port " ) << cc::c( nPort_ );
    return true;
}
void SkaleWsRelay::stop() {
    if ( !isRunning() )
        return;
    clog( dev::VerbosityInfo, cc::info( scheme_uc_ ) )
        << cc::notice( "Will stop on port " ) << cc::c( nPort_ ) << cc::notice( "..." );
    isRunning_ = false;
    waitWhileInLoop();
    close();
    clog( dev::VerbosityInfo, cc::info( scheme_uc_ ) )
        << cc::success( "OK, server stopped on port " ) << cc::c( nPort_ );
}

dev::eth::Interface* SkaleWsRelay::ethereum() const {
    return pso()->ethereum();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

SkaleServerOverride::SkaleServerOverride( dev::eth::Interface* pEth, const std::string& http_addr,
    int http_port, const std::string& web_socket_addr, int web_socket_port,
    const std::string& pathSslKey, const std::string& pathSslCert )
    : AbstractServerConnector(),
      pEth_( pEth ),
      address_http_( http_addr ),
      address_web_socket_( web_socket_addr ),
      port_http_( http_port ),
      port_web_socket_( web_socket_port ),
      bTraceCalls_( false ),
      pathSslKey_( pathSslKey ),
      pathSslCert_( pathSslCert ),
      bIsSSL_( false ) {}

SkaleServerOverride::~SkaleServerOverride() {
    StopListening();
}

dev::eth::Interface* SkaleServerOverride::ethereum() const {
    if ( !pEth_ ) {
        std::cerr << "SLAKLE server fatal error: no eth interface\n";
        std::cerr.flush();
        std::terminate();
    }
    return pEth_;
}

jsonrpc::IClientConnectionHandler* SkaleServerOverride::GetHandler( const std::string& url ) {
    if ( jsonrpc::AbstractServerConnector::GetHandler() != nullptr )
        return AbstractServerConnector::GetHandler();
    std::map< std::string, jsonrpc::IClientConnectionHandler* >::iterator it =
        this->urlhandler.find( url );
    if ( it != this->urlhandler.end() )
        return it->second;
    return nullptr;
}

void SkaleServerOverride::logTraceServerEvent(
    bool isError, const char* strProtocol, const std::string& strMessage ) {
    if ( strMessage.empty() )
        return;
    std::stringstream ssProtocol;
    strProtocol = ( strProtocol && strProtocol[0] ) ? strProtocol : "Unknown network protocol";
    if ( isError )
        ssProtocol << cc::fatal( strProtocol + std::string( " ERROR:" ) );
    else
        ssProtocol << cc::info( strProtocol + std::string( ":" ) );
    if ( isError )
        clog( dev::VerbosityError, ssProtocol.str() ) << strMessage;
    else
        clog( dev::VerbosityInfo, ssProtocol.str() ) << strMessage;
}

void SkaleServerOverride::logTraceServerTraffic( bool isRX, bool isError, const char* strProtocol,
    const char* strOrigin, const std::string& strPayload ) {
    std::stringstream ssProtocol;
    std::string strProto =
        ( strProtocol && strProtocol[0] ) ? strProtocol : "Unknown network protocol";
    strOrigin = ( strOrigin && strOrigin[0] ) ? strOrigin : "unknown origin";
    std::string strErrorSuffix, strOriginSuffix, strDirect;
    if ( isRX ) {
        strDirect = cc::ws_rx( " >>> " );
        ssProtocol << cc::ws_rx_inv( " >>> " + strProto + "/RX >>> " );
    } else {
        strDirect = cc::ws_tx( " <<< " );
        ssProtocol << cc::ws_tx_inv( " <<< " + strProto + "/TX <<< " );
    }
    strOriginSuffix = cc::u( strOrigin );
    if ( isError )
        strErrorSuffix = cc::fatal( " ERROR " );
    if ( isError )
        clog( dev::VerbosityError, ssProtocol.str() )
            << strErrorSuffix << strOriginSuffix << strDirect << strPayload;
    else
        clog( dev::VerbosityInfo, ssProtocol.str() )
            << strErrorSuffix << strOriginSuffix << strDirect << strPayload;
}

bool SkaleServerOverride::startListeningHTTP() {
    try {
        stopListeningHTTP();
        if ( address_http_.empty() || port_http_ <= 0 )
            return true;
        bIsSSL_ = false;
        if ( ( !pathSslKey_.empty() ) && ( !pathSslCert_.empty() ) )
            bIsSSL_ = true;
        logTraceServerEvent( false, bIsSSL_ ? "HTTPS" : "HTTP",
            cc::debug( "starting " ) + cc::info( bIsSSL_ ? "HTTPS" : "HTTP" ) +
                cc::debug( " server on address " ) + cc::info( address_http_ ) +
                cc::debug( " and port " ) + cc::c( port_http_ ) + cc::debug( "..." ) );
        if ( bIsSSL_ )
            this->pServerHTTP_.reset(
                new skutils::http::SSL_server( pathSslCert_.c_str(), pathSslKey_.c_str() ) );
        else
            this->pServerHTTP_.reset( new skutils::http::server );
        this->pServerHTTP_->Options( "/", [&]( const skutils::http::request& req,
                                              skutils::http::response& res ) {
            if ( bTraceCalls_ )
                logTraceServerTraffic( true, false, bIsSSL_ ? "HTTPS" : "HTTP", req.origin_.c_str(),
                    cc::info( "OPTTIONS" ) + cc::debug( " request handler" ) );
            res.set_header( "access-control-allow-headers", "Content-Type" );
            res.set_header( "access-control-allow-methods", "POST" );
            res.set_header( "access-control-allow-origin", "*" );
            res.set_header( "content-length", "0" );
            res.set_header(
                "vary", "Origin, Access-Control-request-Method, Access-Control-request-Headers" );
        } );
        this->pServerHTTP_->Post(
            "/", [this]( const skutils::http::request& req, skutils::http::response& res ) {
                if ( bTraceCalls_ )
                    logTraceServerTraffic( true, false, bIsSSL_ ? "HTTPS" : "HTTP",
                        req.origin_.c_str(), cc::j( req.body_ ) );
                int nID = -1;
                std::string strResponse;
                try {
                    nlohmann::json joRequest = nlohmann::json::parse( req.body_ );
                    nID = joRequest["id"].get< int >();
                    jsonrpc::IClientConnectionHandler* handler = this->GetHandler( "/" );
                    if ( handler == nullptr )
                        throw std::runtime_error( "No client connection handler found" );
                    handler->HandleRequest( req.body_.c_str(), strResponse );
                } catch ( const std::exception& ex ) {
                    logTraceServerTraffic( false, true, bIsSSL_ ? "HTTPS" : "HTTP",
                        req.origin_.c_str(), cc::warn( ex.what() ) );
                    nlohmann::json joErrorResponce;
                    joErrorResponce["id"] = nID;
                    joErrorResponce["result"] = "error";
                    joErrorResponce["error"] = std::string( ex.what() );
                    strResponse = joErrorResponce.dump();
                } catch ( ... ) {
                    const char* e = "unknown exception in SkaleServerOverride";
                    logTraceServerTraffic( false, true, bIsSSL_ ? "HTTPS" : "HTTP",
                        req.origin_.c_str(), cc::warn( e ) );
                    nlohmann::json joErrorResponce;
                    joErrorResponce["id"] = nID;
                    joErrorResponce["result"] = "error";
                    joErrorResponce["error"] = std::string( e );
                    strResponse = joErrorResponce.dump();
                }
                if ( bTraceCalls_ )
                    logTraceServerTraffic( false, false, bIsSSL_ ? "HTTPS" : "HTTP",
                        req.origin_.c_str(), cc::j( strResponse ) );
                res.set_header( "access-control-allow-origin", "*" );
                res.set_header( "vary", "Origin" );
                res.set_content( strResponse.c_str(), "application/json" );
            } );
        std::thread( [this]() {
            this->pServerHTTP_->listen( this->address_http_.c_str(), this->port_http_ );
        } )
            .detach();
        logTraceServerEvent( false, bIsSSL_ ? "HTTPS" : "HTTP",
            cc::success( "OK, started " ) + cc::info( bIsSSL_ ? "HTTPS" : "HTTP" ) +
                cc::success( " server on address " ) + cc::info( address_http_ ) +
                cc::success( " and port " ) + cc::c( port_http_ ) );
        return true;
    } catch ( const std::exception& ex ) {
        logTraceServerEvent( false, bIsSSL_ ? "HTTPS" : "HTTP",
            cc::error( "FAILED to start " ) + cc::warn( bIsSSL_ ? "HTTPS" : "HTTP" ) +
                cc::error( " server: " ) + cc::warn( ex.what() ) );
    } catch ( ... ) {
        logTraceServerEvent( false, bIsSSL_ ? "HTTPS" : "HTTP",
            cc::error( "FAILED to start " ) + cc::warn( bIsSSL_ ? "HTTPS" : "HTTP" ) +
                cc::error( " server: " ) + cc::warn( "unknown exception" ) );
    }
    return false;
}

bool SkaleServerOverride::stopListeningHTTP() {
    try {
        if ( pServerHTTP_ ) {
            logTraceServerEvent( false, bIsSSL_ ? "HTTPS" : "HTTP",
                cc::notice( "Will stop " ) + cc::info( bIsSSL_ ? "HTTPS" : "HTTP" ) +
                    cc::notice( " server on address " ) + cc::info( address_http_ ) +
                    cc::success( " and port " ) + cc::c( port_http_ ) + cc::notice( "..." ) );
            if ( pServerHTTP_->is_running() )
                pServerHTTP_->stop();
            pServerHTTP_.reset();
            logTraceServerEvent( false, bIsSSL_ ? "HTTPS" : "HTTP",
                cc::success( "OK, stopped " ) + cc::info( bIsSSL_ ? "HTTPS" : "HTTP" ) +
                    cc::success( " server on address " ) + cc::info( address_http_ ) +
                    cc::success( " and port " ) + cc::c( port_http_ ) );
        }
    } catch ( ... ) {
    }
    return true;
}

bool SkaleServerOverride::startListeningWebSocket() {
    try {
        stopListeningWebSocket();
        if ( address_web_socket_.empty() || port_web_socket_ <= 0 )
            return true;
        bIsSSL_ = false;
        if ( ( !pathSslKey_.empty() ) && ( !pathSslCert_.empty() ) )
            bIsSSL_ = true;
        logTraceServerEvent( false, bIsSSL_ ? "WSS" : "WS",
            cc::debug( "starting " ) + cc::info( bIsSSL_ ? "WSS" : "WS" ) +
                cc::debug( " server on address " ) + cc::info( address_web_socket_ ) +
                cc::debug( " and port " ) + cc::c( port_web_socket_ ) + cc::debug( "..." ) );
        pServerWS_.reset( new SkaleWsRelay( bIsSSL_ ? "wss" : "ws", port_web_socket_ ) );
        if ( bIsSSL_ ) {
            pServerWS_->strCertificateFile_ = pathSslCert_;
            pServerWS_->strPrivateKeyFile_ = pathSslKey_;
        }
        if ( !pServerWS_->start( this ) )
            throw std::runtime_error( "Failed to start server" );
        logTraceServerEvent( false, bIsSSL_ ? "WSS" : "WS",
            cc::success( "OK, started " ) + cc::info( bIsSSL_ ? "WSS" : "WS" ) +
                cc::success( " server on address " ) + cc::info( address_web_socket_ ) +
                cc::success( " and port " ) + cc::c( port_web_socket_ ) + cc::debug( "..." ) );
        return true;
    } catch ( const std::exception& ex ) {
        logTraceServerEvent( false, bIsSSL_ ? "WSS" : "WS",
            cc::error( "FAILED to start " ) + cc::warn( bIsSSL_ ? "WSS" : "WS" ) +
                cc::error( " server: " ) + cc::warn( ex.what() ) );
    } catch ( ... ) {
        logTraceServerEvent( false, bIsSSL_ ? "WSS" : "WS",
            cc::error( "FAILED to start " ) + cc::warn( bIsSSL_ ? "WSS" : "WS" ) +
                cc::error( " server: " ) + cc::warn( "unknown exception" ) );
    }
    return false;
}
bool SkaleServerOverride::stopListeningWebSocket() {
    try {
        if ( pServerWS_ ) {
            logTraceServerEvent( false, bIsSSL_ ? "WSS" : "WS",
                cc::notice( "Will stop " ) + cc::info( bIsSSL_ ? "WSS" : "WS" ) +
                    cc::notice( " server on address " ) + cc::info( address_web_socket_ ) +
                    cc::success( " and port " ) + cc::c( port_web_socket_ ) + cc::notice( "..." ) );
            if ( pServerWS_->isRunning() )
                pServerWS_->stop();
            pServerWS_.reset();
            logTraceServerEvent( false, bIsSSL_ ? "WSS" : "WS",
                cc::success( "OK, stopped " ) + cc::info( bIsSSL_ ? "WSS" : "WS" ) +
                    cc::success( " server on address " ) + cc::info( address_web_socket_ ) +
                    cc::success( " and port " ) + cc::c( port_web_socket_ ) );
        }
    } catch ( ... ) {
    }
    return true;
}

bool SkaleServerOverride::StartListening() {
    auto retVal = startListeningHTTP() && startListeningWebSocket() ? true : false;
    return retVal;
}

bool SkaleServerOverride::StopListening() {
    auto retVal = stopListeningHTTP() && stopListeningWebSocket() ? true : false;
    return retVal;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
