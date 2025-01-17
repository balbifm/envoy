#include "source/common/quic/client_connection_factory_impl.h"

#include "source/common/quic/quic_transport_socket_factory.h"

#include "quiche/quic/core/crypto/quic_client_session_cache.h"

namespace Envoy {
namespace Quic {

PersistentQuicInfoImpl::PersistentQuicInfoImpl(Event::Dispatcher& dispatcher, uint32_t buffer_limit)
    : conn_helper_(dispatcher), alarm_factory_(dispatcher, *conn_helper_.GetClock()),
      buffer_limit_(buffer_limit) {
  quiche::FlagRegistry::getInstance();
}

std::unique_ptr<PersistentQuicInfoImpl>
createPersistentQuicInfoForCluster(Event::Dispatcher& dispatcher,
                                   const Upstream::ClusterInfo& cluster) {
  auto quic_info = std::make_unique<Quic::PersistentQuicInfoImpl>(
      dispatcher, cluster.perConnectionBufferLimitBytes());
  Quic::convertQuicConfig(cluster.http3Options().quic_protocol_options(), quic_info->quic_config_);
  quic::QuicTime::Delta crypto_timeout =
      quic::QuicTime::Delta::FromMilliseconds(cluster.connectTimeout().count());
  quic_info->quic_config_.set_max_time_before_crypto_handshake(crypto_timeout);
  return quic_info;
}

std::unique_ptr<Network::ClientConnection> createQuicNetworkConnection(
    Http::PersistentQuicInfo& info, std::shared_ptr<quic::QuicCryptoClientConfig> crypto_config,
    const quic::QuicServerId& server_id, Event::Dispatcher& dispatcher,
    Network::Address::InstanceConstSharedPtr server_addr,
    Network::Address::InstanceConstSharedPtr local_addr, QuicStatNames& quic_stat_names,
    OptRef<Http::AlternateProtocolsCache> rtt_cache, Stats::Scope& scope) {
  ASSERT(GetQuicReloadableFlag(quic_single_ack_in_packet2));
  ASSERT(crypto_config != nullptr);
  PersistentQuicInfoImpl* info_impl = reinterpret_cast<PersistentQuicInfoImpl*>(&info);
  quic::ParsedQuicVersionVector quic_versions = quic::CurrentSupportedHttp3Versions();
  ASSERT(!quic_versions.empty());
  auto connection = std::make_unique<EnvoyQuicClientConnection>(
      quic::QuicUtils::CreateRandomConnectionId(), server_addr, info_impl->conn_helper_,
      info_impl->alarm_factory_, quic_versions, local_addr, dispatcher, nullptr);

  // Update config with latest srtt, if available.
  if (rtt_cache.has_value()) {
    Http::AlternateProtocolsCache::Origin origin("https", server_id.host(), server_id.port());
    std::chrono::microseconds rtt = rtt_cache.value().get().getSrtt(origin);
    if (rtt.count() != 0) {
      info_impl->quic_config_.SetInitialRoundTripTimeUsToSend(rtt.count());
    }
  }

  // QUICHE client session always use the 1st version to start handshake.
  return std::make_unique<EnvoyQuicClientSession>(
      info_impl->quic_config_, quic_versions, std::move(connection), server_id,
      std::move(crypto_config), &info_impl->push_promise_index_, dispatcher,
      info_impl->buffer_limit_, info_impl->crypto_stream_factory_, quic_stat_names, rtt_cache,
      scope);
}

} // namespace Quic
} // namespace Envoy
