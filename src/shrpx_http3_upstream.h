/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2021 Tatsuhiro Tsujikawa
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef SHRPX_HTTP3_UPSTREAM_H
#define SHRPX_HTTP3_UPSTREAM_H

#include "shrpx.h"

#include <ngtcp2/ngtcp2.h>
#include <nghttp3/nghttp3.h>

#include "shrpx_upstream.h"
#include "shrpx_downstream_queue.h"
#include "quic.h"
#include "network.h"

using namespace nghttp2;

namespace shrpx {

struct UpstreamAddr;

class Http3Upstream : public Upstream {
public:
  Http3Upstream(ClientHandler *handler);
  virtual ~Http3Upstream();

  virtual int on_read();
  virtual int on_write();
  virtual int on_timeout(Downstream *downstream);
  virtual int on_downstream_abort_request(Downstream *downstream,
                                          unsigned int status_code);
  virtual int
  on_downstream_abort_request_with_https_redirect(Downstream *downstream);
  virtual int downstream_read(DownstreamConnection *dconn);
  virtual int downstream_write(DownstreamConnection *dconn);
  virtual int downstream_eof(DownstreamConnection *dconn);
  virtual int downstream_error(DownstreamConnection *dconn, int events);
  virtual ClientHandler *get_client_handler() const;

  virtual int on_downstream_header_complete(Downstream *downstream);
  virtual int on_downstream_body(Downstream *downstream, const uint8_t *data,
                                 size_t len, bool flush);
  virtual int on_downstream_body_complete(Downstream *downstream);

  virtual void on_handler_delete();
  virtual int on_downstream_reset(Downstream *downstream, bool no_retry);

  virtual void pause_read(IOCtrlReason reason);
  virtual int resume_read(IOCtrlReason reason, Downstream *downstream,
                          size_t consumed);
  virtual int send_reply(Downstream *downstream, const uint8_t *body,
                         size_t bodylen);

  virtual int initiate_push(Downstream *downstream, const StringRef &uri);

  virtual int response_riovec(struct iovec *iov, int iovcnt) const;
  virtual void response_drain(size_t n);
  virtual bool response_empty() const;

  virtual Downstream *on_downstream_push_promise(Downstream *downstream,
                                                 int32_t promised_stream_id);
  virtual int
  on_downstream_push_promise_complete(Downstream *downstream,
                                      Downstream *promised_downstream);
  virtual bool push_enabled() const;
  virtual void cancel_premature_downstream(Downstream *promised_downstream);

  int init(const UpstreamAddr *faddr, const Address &remote_addr,
           const Address &local_addr, const ngtcp2_pkt_hd &initial_hd,
           const ngtcp2_cid *odcid, const uint8_t *token, size_t tokenlen);

  int on_read(const UpstreamAddr *faddr, const Address &remote_addr,
              const Address &local_addr, const ngtcp2_pkt_info &pi,
              const uint8_t *data, size_t datalen);

  int write_streams();

  int on_rx_secret(ngtcp2_crypto_level level, const uint8_t *secret,
                   size_t secretlen);
  int on_tx_secret(ngtcp2_crypto_level level, const uint8_t *secret,
                   size_t secretlen);

  int add_crypto_data(ngtcp2_crypto_level level, const uint8_t *data,
                      size_t datalen);

  void set_tls_alert(uint8_t alert);

  int handle_error();

  int handle_expiry();
  void reset_idle_timer();
  void reset_timer();

  int setup_httpconn();
  void add_pending_downstream(std::unique_ptr<Downstream> downstream);
  int recv_stream_data(uint32_t flags, int64_t stream_id, const uint8_t *data,
                       size_t datalen);
  int acked_stream_data_offset(int64_t stream_id, uint64_t datalen);
  int extend_max_stream_data(int64_t stream_id);
  void extend_max_remote_streams_bidi(uint64_t max_streams);
  int error_reply(Downstream *downstream, unsigned int status_code);
  void http_begin_request_headers(int64_t stream_id);
  int http_recv_request_header(Downstream *downstream, int32_t token,
                               nghttp3_rcbuf *name, nghttp3_rcbuf *value,
                               uint8_t flags);
  int http_end_request_headers(Downstream *downstream);
  int http_end_stream(Downstream *downstream);
  void start_downstream(Downstream *downstream);
  void initiate_downstream(Downstream *downstream);
  int shutdown_stream(Downstream *downstream, uint64_t app_error_code);
  int shutdown_stream_read(int64_t stream_id, uint64_t app_error_code);
  int redirect_to_https(Downstream *downstream);
  int http_stream_close(Downstream *downstream, uint64_t app_error_code);
  void consume(int64_t stream_id, size_t nconsumed);
  void remove_downstream(Downstream *downstream);
  int stream_close(int64_t stream_id, uint64_t app_error_code);
  void log_response_headers(Downstream *downstream,
                            const std::vector<nghttp3_nv> &nva) const;
  int http_acked_stream_data(Downstream *downstream, uint64_t datalen);
  int http_shutdown_stream_read(int64_t stream_id);
  int http_reset_stream(int64_t stream_id, uint64_t app_error_code);
  int http_stop_sending(int64_t stream_id, uint64_t app_error_code);
  int http_recv_data(Downstream *downstream, const uint8_t *data,
                     size_t datalen);
  int handshake_completed();
  int check_shutdown();
  int start_graceful_shutdown();
  int submit_goaway();
  void idle_close();
  int send_packet(const UpstreamAddr *faddr, const sockaddr *remote_sa,
                  size_t remote_salen, const sockaddr *local_sa,
                  size_t local_salen, const ngtcp2_pkt_info &pi,
                  const uint8_t *data, size_t datalen, size_t gso_size);

  void qlog_write(const void *data, size_t datalen, bool fin);
  int open_qlog_file(const StringRef &dir, const ngtcp2_cid &scid) const;

private:
  ClientHandler *handler_;
  ev_timer timer_;
  ev_timer idle_timer_;
  ev_timer shutdown_timer_;
  ev_prepare prep_;
  size_t max_udp_payload_size_;
  int qlog_fd_;
  ngtcp2_cid hashed_scid_;
  ngtcp2_conn *conn_;
  quic::Error last_error_;
  uint8_t tls_alert_;
  nghttp3_conn *httpconn_;
  DownstreamQueue downstream_queue_;
  bool idle_close_;
  bool retry_close_;
  std::vector<uint8_t> conn_close_;
};

} // namespace shrpx

#endif // SHRPX_HTTP3_UPSTREAM_H
