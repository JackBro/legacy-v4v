/* Copyright (c) Citrix Systems Inc.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, 
 * with or without modification, are permitted provided 
 * that the following conditions are met:
 * 
 * *   Redistributions of source code must retain the above 
 *     copyright notice, this list of conditions and the 
 *     following disclaimer.
 * *   Redistributions in binary form must reproduce the above 
 *     copyright notice, this list of conditions and the 
 *     following disclaimer in the documentation and/or other 
 *     materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND 
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, 
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR 
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE.
 */ 

#ifndef XSAPI_FUTURE_H__
#define XSAPI_FUTURE_H__

#ifdef XSAPI_FUTURE_CONNECT_EVTCHN

/* A wrapper type for event channel ports which have been offered to
 * us by a remote domain.  Any given ALIEN_EVTCHN_PORT is only
 * meaningful when interpreted with respect to a particular remote
 * DOMAIN_ID, and it is the caller's responsibility to track which
 * domain a particular grant reference is measured against.
 */
MAKE_WRAPPER_PUB(ALIEN_EVTCHN_PORT)

/* Read an alien event channel port from @prefix/@node under
 * transaction @xbt, and store it in *@port.  @prefix/@node is
 * expected to contain a non-negative base-10 integer less than 2^32,
 * and this is used as the remote port number when communicating with
 * Xen.  For remote Windows VMs, the store node should have been
 * populated with xenbus_write_evtchn_port(); other guest operating
 * systems will provide analogous APIs.
 *
 * On error, *@port is set to null_ALIEN_EVTCHN_PORT().
 *
 * @xbt may be nil, null, or a valid transaction.
 */
XSAPI NTSTATUS xenbus_read_evtchn_port(xenbus_transaction_t xbt, PCSTR prefix,
                                       PCSTR node, ALIEN_EVTCHN_PORT *port);

/* Bind the alien event channel port @port in domain @domid to a local
 * event channel port, and arrange that @cb will be called with
 * argument @context shortly after the remote domain notifies the
 * port.  The local event channel port is returned.
 *
 * The local port has semantics broadly analogous to those of
 * EvtchnAllocUnboundDpc():
 *
 * -- The port cannot be masked with EvtchnPortMask(), or unmasked with
 *    EvtchnPortUnmask().
 * -- The callback is run from a DPC.  The details of how this is done
 *    are not defined; in particular, there is no guarantee that there is
 *    a one-to-one correspondence between EVTCHN_PORTs and Windows DPCs.
 * -- It is guaranteed that a single port will only fire on one CPU at
 *    a time.  However, the library may fire different ports in parallel.
 * -- The port may be fired spuriously at any time.
 * -- There is no guarantee that every notification issued by the
 *    remote will cause precisely one invocation of the callback.  In
 *    particular, if the remote notifies the port several times in quick
 *    succession, the events may be aggregated into a single callback.
 *    There is no general way to detect that this has happened.
 *
 * There is no way to run a remote port callback directly from the
 * interrupt handler.
 *
 * The remote domain may close the alien event channel port at any
 * time.  If that happens before the call to EvtchnConnectRemotePort()
 * completes, it returns an error.  If it happens after the call
 * completes, there is no way for the local domain to tell, and
 * notifications to the port are simply dropped.
 *
 * If the local domain suspend and resumes, migrates, or hibernates
 * and restores, the library will attempt to automatically reconnect
 * the port.  This may, of course, fail, in which case we behave as if
 * the remote domain had closed the port.
 *
 * The port should be closed with EvtchnClose() once it is no longer
 * needed.
 *
 * Call at PASSIVE_LEVEL.
 */
XSAPI EVTCHN_PORT EvtchnConnectRemotePort(DOMAIN_ID domid,
                                          ALIEN_EVTCHN_PORT port,
                                          PEVTCHN_HANDLER_CB cb,
                                          void *context);
#endif /* XSAPI_FUTURE_CONNECT_EVTCHN */

#endif /* !XSAPI_FUTURE_H__ */
