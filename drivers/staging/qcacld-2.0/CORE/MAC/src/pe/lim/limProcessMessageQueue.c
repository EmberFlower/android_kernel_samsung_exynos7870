/*
 * Copyright (c) 2011-2015 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */


/*
 * This file lim ProcessMessageQueue.cc contains the code
 * for processing LIM message Queue.
 * Author:        Chandra Modumudi
 * Date:          02/11/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */
#include "palTypes.h"
#include "wniApi.h"
#include "wlan_qct_wda.h"

#include "wniCfgSta.h"
#include "cfgApi.h"
#include "sirCommon.h"
#include "utilsApi.h"
#include "limTypes.h"
#include "limUtils.h"
#include "limAssocUtils.h"
#include "limPropExtsUtils.h"

#include "limAdmitControl.h"
#include "pmmApi.h"
#include "limIbssPeerMgmt.h"
#include "schApi.h"
#ifdef WLAN_FEATURE_VOWIFI_11R
#include "limFTDefs.h"
#endif
#include "limSession.h"
#include "limSendMessages.h"

#if defined WLAN_FEATURE_VOWIFI
#include "rrmApi.h"
#endif
#if defined(FEATURE_WLAN_ESE) && !defined(FEATURE_WLAN_ESE_UPLOAD)
#include "eseApi.h"
#endif

#if defined WLAN_FEATURE_VOWIFI_11R
#include "limFT.h"
#endif

#include "vos_types.h"
#include "vos_packet.h"
#include "vos_memory.h"

void limLogSessionStates(tpAniSirGlobal pMac);

/** -------------------------------------------------------------
\fn defMsgDecision
\brief The function decides whether to defer a message or not in limProcessMessage function
\param   tpAniSirGlobal pMac
\param       tSirMsgQ  limMsg
\param       tSirMacTspecIE   *ppInfo
\return none
  -------------------------------------------------------------*/

tANI_U8 static
defMsgDecision(tpAniSirGlobal pMac, tpSirMsgQ  limMsg)
{


/* this function should not changed */
  if(pMac->lim.gLimSmeState == eLIM_SME_OFFLINE_STATE)
  {
      // Defer processsing this message
      if (limDeferMsg(pMac, limMsg) != TX_SUCCESS)
      {
          PELOGW(limLog(pMac, LOGW, FL("Unable to Defer message(0x%X) limSmeState %d (prev sme state %d) sysRole %d mlm state %d (prev mlm state %d)"),
                   limMsg->type, pMac->lim.gLimSmeState,  pMac->lim.gLimPrevSmeState,
                   pMac->lim.gLimSystemRole,  pMac->lim.gLimMlmState,  pMac->lim.gLimPrevMlmState);)
          limLogSessionStates(pMac);
          limHandleDeferMsgError(pMac, limMsg);
      }
      return true;
  }

  //When defer is requested then defer all the messages except HAL responses.
  if((!limIsSystemInScanState(pMac)) && (true != GET_LIM_PROCESS_DEFD_MESGS(pMac)) &&
      !pMac->lim.gLimSystemInScanLearnMode)
  {
      if((limMsg->type != WDA_ADD_BSS_RSP) &&
        (limMsg->type != WDA_DELETE_BSS_RSP) &&
        (limMsg->type != WDA_ADD_STA_RSP) &&
        (limMsg->type != WDA_ADD_STA_SELF_RSP) &&
        (limMsg->type != WDA_DEL_STA_SELF_RSP) &&
        (limMsg->type != WDA_DELETE_STA_RSP)&&
        (limMsg->type != WDA_SET_BSSKEY_RSP)&&
        (limMsg->type != WDA_SET_STAKEY_RSP)&&
        (limMsg->type != WDA_SET_STA_BCASTKEY_RSP) &&
        (limMsg->type != eWNI_SME_START_REQ) &&
        (limMsg->type != WDA_AGGR_QOS_RSP) &&
        (limMsg->type != WDA_REMOVE_BSSKEY_RSP) &&
        (limMsg->type != WDA_REMOVE_STAKEY_RSP) &&
        (limMsg->type != WDA_SET_MIMOPS_RSP)&&
        (limMsg->type != WDA_ADDBA_RSP) &&
        (limMsg->type != WDA_ENTER_BMPS_RSP) &&
        (limMsg->type != WDA_EXIT_BMPS_RSP) &&
        (limMsg->type != WDA_ENTER_IMPS_RSP) &&
        (limMsg->type != WDA_EXIT_IMPS_RSP) &&
        (limMsg->type != WDA_ENTER_UAPSD_RSP) &&
        (limMsg->type != WDA_EXIT_UAPSD_RSP) &&
        (limMsg->type != WDA_WOWL_ENTER_RSP) &&
        (limMsg->type != WDA_WOWL_EXIT_RSP) &&
        (limMsg->type != WDA_SWITCH_CHANNEL_RSP) &&
        (limMsg->type != WDA_P2P_NOA_ATTR_IND) &&
        (limMsg->type != WDA_P2P_NOA_START_IND) &&
#ifdef FEATURE_OEM_DATA_SUPPORT
        (limMsg->type != WDA_START_OEM_DATA_RSP) &&
#endif
        (limMsg->type != WDA_ADD_TS_RSP) &&
        /* Allow processing of RX frames while awaiting reception of
           ADD TS response over the air. This logic particularly handles the
           case when host sends ADD BA request to FW after ADD TS request
           is sent over the air and ADD TS response received over the air */
        !(limMsg->type == SIR_BB_XPORT_MGMT_MSG && pMac->lim.gLimAddtsSent)) {
        PELOG1(limLog(pMac, LOG1, FL("Defer the current message type %d , gLimProcessDefdMsgs is false and system is not in scan/learn mode"),
               limMsg->type);)

        // Defer processsing this message
        if (limDeferMsg(pMac, limMsg) != TX_SUCCESS)
        {
            PELOGW(limLog(pMac, LOGW, FL("Unable to Defer message(0x%X) limSmeState %d (prev sme state %d) sysRole %d mlm state %d (prev mlm state %d)"),
                   limMsg->type, pMac->lim.gLimSmeState,  pMac->lim.gLimPrevSmeState,
                   pMac->lim.gLimSystemRole,  pMac->lim.gLimMlmState,  pMac->lim.gLimPrevMlmState);)
            limLogSessionStates(pMac);
            limHandleDeferMsgError(pMac, limMsg);

        }
        return true;
    }
  }
  return false;
}

#ifdef FEATURE_WLAN_EXTSCAN
static void
__lim_pno_match_fwd_bcn_probepsp(tpAniSirGlobal pmac, uint8_t *rx_pkt_info,
				tSirProbeRespBeacon *frame, uint32_t ie_len,
				uint32_t msg_type)
{
	struct pno_match_found  *result;
	uint8_t                 *body;
	tSirMsgQ                mmh_msg;
	tpSirMacMgmtHdr         hdr;
	uint32_t num_results = 1, len, i;

	/* Upon receiving every matched beacon, bss info is forwarded to the
	 * the upper layer, hence num_results is set to 1 */
	len = sizeof(*result) + (num_results * sizeof(tSirWifiScanResult)) +
		ie_len;

	result = vos_mem_malloc(len);
	if (NULL == result) {
		limLog(pmac, LOGE, FL("Memory allocation failed"));
		return;
	}
	hdr = WDA_GET_RX_MAC_HEADER(rx_pkt_info);
	body = WDA_GET_RX_MPDU_DATA(rx_pkt_info);
	vos_mem_zero(result, sizeof(*result) + ie_len);

	/* Received frame does not have request id, hence set 0 */
	result->request_id = 0;
	result->more_data = 0;
	result->num_results = num_results;

	for (i = 0; i < result->num_results; i++) {
		result->ap[i].ts = vos_timer_get_system_time();
		result->ap[i].beaconPeriod = frame->beaconInterval;
		result->ap[i].capability =
			limGetU16((uint8_t *)&frame->capabilityInfo);
		result->ap[i].channel = WDA_GET_RX_CH(rx_pkt_info);
		result->ap[i].rssi = WDA_GET_RX_RSSI_NORMALIZED(rx_pkt_info);
		result->ap[i].rtt = 0;
		result->ap[i].rtt_sd = 0;
		result->ap[i].ieLength = ie_len;
		vos_mem_copy((uint8_t *) &result->ap[i].ssid[0],
                 (uint8_t *) frame->ssId.ssId, frame->ssId.length);
		result->ap[i].ssid[frame->ssId.length] = '\0';
		vos_mem_copy((uint8_t *) &result->ap[i].bssid,
				(uint8_t *) hdr->bssId,
				sizeof(tSirMacAddr));
		/* Copy IE fields */
		vos_mem_copy((uint8_t *) &result->ap[i].ieData,
				body + SIR_MAC_B_PR_SSID_OFFSET, ie_len);
	}

	mmh_msg.type = msg_type;
	mmh_msg.bodyptr = result;
	mmh_msg.bodyval = 0;
	limSysProcessMmhMsgApi(pmac, &mmh_msg, ePROT);
}

static void
__limExtScanForwardBcnProbeRsp(tpAniSirGlobal pmac, uint8_t *rx_pkt_info,
				tSirProbeRespBeacon *frame, uint32_t ie_len,
				uint32_t msg_type)
{
	tpSirWifiFullScanResultEvent result;
	uint8_t                     *body;
	tSirMsgQ                     mmh_msg;
	tpSirMacMgmtHdr              hdr;

	result = vos_mem_malloc(sizeof(*result) + ie_len);
	if (NULL == result) {
		limLog(pmac, LOGE, FL("Memory allocation failed"));
		return;
	}
	hdr = WDA_GET_RX_MAC_HEADER(rx_pkt_info);
	body = WDA_GET_RX_MPDU_DATA(rx_pkt_info);
	vos_mem_zero(result, sizeof(*result) + ie_len);

	/* Received frame does not have request id, hence set 0 */
	result->requestId = 0;

	result->moreData = 0;
	result->ap.ts = vos_timer_get_system_time();
	result->ap.beaconPeriod = frame->beaconInterval;
	result->ap.capability = limGetU16((uint8_t *)&frame->capabilityInfo);
	result->ap.channel = WDA_GET_RX_CH(rx_pkt_info);
	result->ap.rssi = WDA_GET_RX_RSSI_NORMALIZED(rx_pkt_info);
	result->ap.rtt = 0;
	result->ap.rtt_sd = 0;
	result->ap.ieLength = ie_len;

	vos_mem_copy((uint8_t *) &result->ap.ssid[0],
                 (uint8_t *) frame->ssId.ssId, frame->ssId.length);
	result->ap.ssid[frame->ssId.length] = '\0';
	vos_mem_copy((uint8_t *) &result->ap.bssid, (uint8_t *) hdr->bssId,
			sizeof(tSirMacAddr));
	/* Copy IE fields */
	vos_mem_copy((uint8_t *) &result->ap.ieData,
			body + SIR_MAC_B_PR_SSID_OFFSET, ie_len);

	mmh_msg.type = msg_type;
	mmh_msg.bodyptr = result;
	mmh_msg.bodyval = 0;
	limSysProcessMmhMsgApi(pmac, &mmh_msg, ePROT);
}

static void
__limProcessExtScanBeaconProbeRsp(tpAniSirGlobal pmac, uint8_t *rx_pkt_info,
                                  uint8_t sub_type)
{
	tSirProbeRespBeacon  *frame;
	uint8_t              *body;
	uint32_t             frm_len;
	tSirRetStatus        status;

	frm_len = WDA_GET_RX_PAYLOAD_LEN(rx_pkt_info);
	if (frm_len <= SIR_MAC_B_PR_SSID_OFFSET) {
		limLog(pmac, LOGP,
			FL("RX packet has invalid length %d"), frm_len);
		return;
	}

	frame = vos_mem_malloc(sizeof(*frame));
	if (NULL == frame) {
		limLog(pmac, LOGE, FL("Memory allocation failed"));
		return;
	}

	if (sub_type == SIR_MAC_MGMT_BEACON) {
		limLog(pmac, LOG2, FL("Beacon due to ExtScan/epno"));
		status = sirConvertBeaconFrame2Struct(pmac,
						(uint8_t *)rx_pkt_info,
						frame);
	} else if (sub_type == SIR_MAC_MGMT_PROBE_RSP) {
		limLog(pmac, LOG2, FL("Probe Rsp due to ExtScan/epno"));
		body = WDA_GET_RX_MPDU_DATA(rx_pkt_info);
		status = sirConvertProbeFrame2Struct(pmac, body,
							frm_len, frame);
	} else {
		vos_mem_free(frame);
		return;
	}

	if (status != eSIR_SUCCESS) {
		limLog(pmac, LOGE, FL("Frame parsing failed"));
		vos_mem_free(frame);
		return;
	}

	if (WMA_IS_EXTSCAN_SCAN_SRC(rx_pkt_info))
		__limExtScanForwardBcnProbeRsp(pmac, rx_pkt_info, frame,
					(frm_len - SIR_MAC_B_PR_SSID_OFFSET),
					eWNI_SME_EXTSCAN_FULL_SCAN_RESULT_IND);

	if (WMA_IS_EPNO_SCAN_SRC(rx_pkt_info))
		__lim_pno_match_fwd_bcn_probepsp(pmac, rx_pkt_info, frame,
					(frm_len - SIR_MAC_B_PR_SSID_OFFSET),
					eWNI_SME_EPNO_NETWORK_FOUND_IND);

	vos_mem_free(frame);
}
#endif


/*
* Beacon Handling Cases:
* during scanning, when no session is active:
*    handled by limHandleFramesInScanState before __limHandleBeacon call is invoked.
* during scanning, when any session is active, but beacon/Pr does not belong to that session, psessionEntry will be null.
*    handled by limHandleFramesInScanState before __limHandleBeacon call is invoked.
* during scanning, when any session is active, and beacon/Pr belongs to one of the session, psessionEntry will not be null.
*    handled by limHandleFramesInScanState before __limHandleBeacon call is invoked.
* Not scanning, no session:
*    there should not be any beacon coming, if coming, should be dropped.
* Not Scanning,
*/
static void
__limHandleBeacon(tpAniSirGlobal pMac, tpSirMsgQ pMsg, tpPESession psessionEntry)
{
    /* checking for global SME state...*/
    tANI_U8 *pRxPacketInfo;
    limGetBDfromRxPacket(pMac, pMsg->bodyptr, (tANI_U32 **)&pRxPacketInfo);

    //This function should not be called if beacon is received in scan state.
    //So not doing any checks for the global state.

    if(psessionEntry == NULL)
    {
        schBeaconProcess(pMac, pRxPacketInfo, NULL);
    }
    else if( (psessionEntry->limSmeState == eLIM_SME_LINK_EST_STATE) ||
                (psessionEntry->limSmeState == eLIM_SME_NORMAL_STATE))
    {
        schBeaconProcess(pMac, pRxPacketInfo, psessionEntry);
    }
     else
        limProcessBeaconFrame(pMac, pRxPacketInfo, psessionEntry);

        return;
}


//Fucntion prototype
void limProcessNormalHddMsg(tpAniSirGlobal pMac, tSirMsgQ *pLimMsg, tANI_U8 fRspReqd);

/**
 * limDeferMsg()
 *
 *FUNCTION:
 * This function is called to defer the messages received
 * during Learn mode
 *
 *LOGIC:
 * NA
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pMac - Pointer to Global MAC structure
 * @param  pMsg of type tSirMsgQ - Pointer to the message structure
 * @return None
 */

tANI_U32
limDeferMsg(tpAniSirGlobal pMac, tSirMsgQ *pMsg)
{
    tANI_U32 retCode = TX_SUCCESS;

    retCode = limWriteDeferredMsgQ(pMac, pMsg);

    if (retCode == TX_SUCCESS)
    {
        limLog(pMac, LOG1,
              FL("Deferred message(0x%X) limSmeState %d (prev sme state %d)"
              " sysRole %d mlm state %d (prev mlm state %d)"),
              pMsg->type, pMac->lim.gLimSmeState, pMac->lim.gLimPrevSmeState,
              pMac->lim.gLimSystemRole, pMac->lim.gLimMlmState,
              pMac->lim.gLimPrevMlmState);
        MTRACE(macTraceMsgRx(pMac, NO_SESSION, LIM_TRACE_MAKE_RXMSG(pMsg->type, LIM_MSG_DEFERRED));)
    }
    else
    {
        limLog(pMac, LOG1, FL("Dropped lim message (0x%X)"), pMsg->type);
        MTRACE(macTraceMsgRx(pMac, NO_SESSION, LIM_TRACE_MAKE_RXMSG(pMsg->type, LIM_MSG_DROPPED));)
    }

    return retCode;
} /*** end limDeferMsg() ***/



/**
 * limHandleFramesInScanState()
 *
 *FUNCTION:
 * This function is called to process 802.11 frames
 * received by LIM in scan state.
 *
 *LOGIC:
 * NA
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pMac    - Pointer to Global MAC structure
 * @param  limMsg  - Received message
 * @param  pRxPacketInfo - Pointer to Rx packet info structure
 * @param  deferMsg - Indicates whether the frame shall be deferred
 * @return None
 */

static void
limHandleFramesInScanState(tpAniSirGlobal pMac, tpSirMsgQ limMsg, tANI_U8 *pRxPacketInfo, tANI_U8 *deferMsg, tpPESession psessionEntry)
{
    tSirMacFrameCtl  fc;
    tpSirMacMgmtHdr  pHdr;

    *deferMsg = false;
    pHdr = WDA_GET_RX_MAC_HEADER(pRxPacketInfo);
    fc = pHdr->fc;
    limLog( pMac, LOG2, FL("ProtVersion %d, Type %d, Subtype %d"),
            fc.protVer, fc.type, fc.subType );

    // defer all message in scan state except for Beacons and Probe Response
    if ((fc.type == SIR_MAC_MGMT_FRAME) && (fc.subType == SIR_MAC_MGMT_BEACON))
    {
        if (psessionEntry == NULL)
            limProcessBeaconFrameNoSession(pMac, pRxPacketInfo);
        else
            limProcessBeaconFrame(pMac, pRxPacketInfo,psessionEntry);
    }
    else if ((fc.type == SIR_MAC_MGMT_FRAME) && (fc.subType == SIR_MAC_MGMT_PROBE_RSP))
    {
        if (psessionEntry == NULL)
            limProcessProbeRspFrameNoSession(pMac, pRxPacketInfo);
        else
            limProcessProbeRspFrame(pMac, pRxPacketInfo,psessionEntry);
    }
    else if ((fc.type == SIR_MAC_MGMT_FRAME) && (fc.subType == SIR_MAC_MGMT_PROBE_REQ))
    {
      limProcessProbeReqFrame_multiple_BSS(pMac, pRxPacketInfo, psessionEntry);
    }
    else if ((fc.type == SIR_MAC_MGMT_FRAME) && (fc.subType == SIR_MAC_MGMT_ACTION))
    {
       limProcessActionFrameNoSession( pMac, pRxPacketInfo);
    }
    else
    {
        *deferMsg = true;
        return;
    }

    limPktFree(pMac, HAL_TXRX_FRM_802_11_MGMT, pRxPacketInfo, (void *) limMsg->bodyptr);
    return;

} /*** end limHandleFramesInScanState() ***/

/** ------------------------------------------------------------
\brief    This function handles Unknown Unicast (A2 Index)
\         packets.
\param    tpAniSirGlobal pMac Global Mac data structure
\param    void *pRxPacketInfo Pointer to Buffer Descriptor
\return   none
\
\ -------------------------------------------------------------- */
static void limHandleUnknownA2IndexFrames(tpAniSirGlobal pMac, void *pRxPacketInfo,tpPESession psessionEntry)
{
      /* addr2 mismatch interrupt occurred this means previous
       disassociation was not successful
       In Volans pRxPacketInfo only contains pointer 48-bit address2 field */
       /*Send disassociation message again*/
       //Dinesh need one more arguement.
       //limSendDisassocMgmtFrame(pMac, eSIR_MAC_CLASS3_FRAME_FROM_NON_ASSOC_STA_REASON,(tANI_U8 *) pRxPacketInfo);
    //TODO: verify this
    //This could be a public action frame.
    if (LIM_IS_P2P_DEVICE_ROLE(psessionEntry))
        limProcessActionFrameNoSession(pMac, (tANI_U8 *) pRxPacketInfo);

#ifdef FEATURE_WLAN_TDLS
    {
        tpSirMacDataHdr3a pMacHdr;
        pMacHdr = WDA_GET_RX_MPDUHEADER3A(pRxPacketInfo);

        if (limIsGroupAddr(pMacHdr->addr2))
        {
            PELOG2(limLog(pMac, LOG2, FL("Ignoring A2 Invalid Packet received for MC/BC:"));
                    limPrintMacAddr(pMac, pMacHdr->addr2, LOG2);)

                return;
        }
        /* TDLS_hklee: move down here to reject Addr2 == Group (first checking above)
           and also checking if SystemRole == STA */
        if (LIM_IS_STA_ROLE(psessionEntry))
        {
            /* ADD handling of Public Action Frame */
            LIM_LOG_TDLS(VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR,
                        ("limHandleUnknownA2IndexFrames: type=0x%x, subtype=0x%x"),
                        pMacHdr->fc.type, pMacHdr->fc.subType));
            switch (pMacHdr->fc.type)
            {
                case SIR_MAC_MGMT_FRAME:
                    {
                        switch (pMacHdr->fc.subType)
                        {
                            case SIR_MAC_MGMT_ACTION:
                                {
                                    limProcessActionFrame(pMac, pRxPacketInfo, psessionEntry) ;
                                    break ;
                                }
                            default:
                                {
                                    break ;
                                }
                        }
                    }
                default:
                    {
                        break ;
                    }
            }
        }
    }
#endif


    return;
}

/**
 * limCheckMgmtRegisteredFrames()
 *
 *FUNCTION:
 * This function is called to process to check if received frame match with
 * any of the registered frame from HDD. If yes pass this frame to SME.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac      Pointer to Global MAC structure
 * @param  *pBd     Pointer to the received Buffer Descriptor+payload
 * @param  *psessionEntry Pointer to session on which packet is received
 * @return None
 */
static tANI_BOOLEAN
limCheckMgmtRegisteredFrames(tpAniSirGlobal pMac, tANI_U8 *pBd,
                                        tpPESession psessionEntry)
{
    tSirMacFrameCtl  fc;
    tpSirMacMgmtHdr  pHdr;
    tANI_U8          *pBody;
    tpLimMgmtFrameRegistration pLimMgmtRegistration = NULL, pNext = NULL;
    tANI_U16 frameType;
    tANI_U16 framelen;
    tANI_U8 type,subType;
    tANI_BOOLEAN match = VOS_FALSE;
    VOS_STATUS vosStatus;

    pHdr = WDA_GET_RX_MAC_HEADER(pBd);
    fc = pHdr->fc;
    frameType = (fc.type << 2 ) | (fc.subType << 4);
    pBody = WDA_GET_RX_MPDU_DATA(pBd);
    framelen = WDA_GET_RX_PAYLOAD_LEN(pBd);

    vos_list_peek_front(&pMac->lim.gLimMgmtFrameRegistratinQueue,
                        (vos_list_node_t**)&pLimMgmtRegistration);

    while(pLimMgmtRegistration != NULL)
    {
        type = (pLimMgmtRegistration->frameType >> 2) & 0x03;
        subType = (pLimMgmtRegistration->frameType >> 4) & 0x0f;
        if ( (type == SIR_MAC_MGMT_FRAME) && (fc.type == SIR_MAC_MGMT_FRAME)
              && (subType == SIR_MAC_MGMT_RESERVED15) )
        {
            limLog( pMac, LOG3,
                FL("rcvd frame match with SIR_MAC_MGMT_RESERVED15"));
            match = VOS_TRUE;
            break;
        }

        if (pLimMgmtRegistration->frameType == frameType)
        {
            if (pLimMgmtRegistration->matchLen > 0)
            {
                if (pLimMgmtRegistration->matchLen <= framelen)
                {
                    if (vos_mem_compare(pLimMgmtRegistration->matchData,
                                       pBody, pLimMgmtRegistration->matchLen))
                    {
                         /* found match! */
                         match = VOS_TRUE;
                         break;
                    }
                }
            }
            else
            {
                /* found match! */
                match = VOS_TRUE;
                break;
            }
        }

        vosStatus =
          vos_list_peek_next ( &pMac->lim.gLimMgmtFrameRegistratinQueue,
                              (vos_list_node_t*) pLimMgmtRegistration,
                              (vos_list_node_t**) &pNext );
        pLimMgmtRegistration = pNext;
        pNext = NULL;
    }

    if (match)
    {
        limLog( pMac, LOG1,
                FL("rcvd frame match with registered frame params"));

        /* Indicate this to SME */
        limSendSmeMgmtFrameInd( pMac, pHdr->fc.subType, (tANI_U8*)pHdr,
                     WDA_GET_RX_PAYLOAD_LEN(pBd) + sizeof(tSirMacMgmtHdr),
                     pLimMgmtRegistration->sessionId,
                     WDA_GET_RX_CH(pBd), psessionEntry, 0);

        if ( (type == SIR_MAC_MGMT_FRAME) && (fc.type == SIR_MAC_MGMT_FRAME)
              && (subType == SIR_MAC_MGMT_RESERVED15) )
        {
            // These packets needs to be processed by PE/SME as well as HDD.
            // If it returns TRUE here, the packet is forwarded to HDD only.
            match = VOS_FALSE;
        }
    }

    return match;
} /*** end  limCheckMgmtRegisteredFrames() ***/


/**
 * limHandle80211Frames()
 *
 *FUNCTION:
 * This function is called to process 802.11 frames
 * received by LIM.
 *
 *LOGIC:
 * NA
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pMac - Pointer to Global MAC structure
 * @param  pMsg of type tSirMsgQ - Pointer to the message structure
 * @return None
 */

static void
limHandle80211Frames(tpAniSirGlobal pMac, tpSirMsgQ limMsg, tANI_U8 *pDeferMsg)
{
    tANI_U8          *pRxPacketInfo = NULL;
    tSirMacFrameCtl  fc;
    tpSirMacMgmtHdr    pHdr=NULL;
    tpPESession         psessionEntry=NULL;
    tANI_U8             sessionId;
    tAniBool            isFrmFt = FALSE;
    tANI_U16            fcOffset = WLANHAL_RX_BD_HEADER_SIZE;

    *pDeferMsg= false;
    limGetBDfromRxPacket(pMac, limMsg->bodyptr, (tANI_U32 **)&pRxPacketInfo);

    pHdr = WDA_GET_RX_MAC_HEADER(pRxPacketInfo);
    isFrmFt = WDA_GET_RX_FT_DONE(pRxPacketInfo);
    fcOffset = (v_U8_t)WDA_GET_RX_MPDU_HEADER_OFFSET(pRxPacketInfo);
    fc = pHdr->fc;

#ifdef WLAN_DUMP_MGMTFRAMES
    limLog( pMac, LOGE, FL("ProtVersion %d, Type %d, Subtype %d rateIndex=%d"),
            fc.protVer, fc.type, fc.subType,
            WDA_GET_RX_MAC_RATE_IDX(pRxPacketInfo));
    VOS_TRACE_HEX_DUMP(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR, pHdr,
                       WDA_GET_RX_MPDU_HEADER_LEN(pRxPacketInfo));
#endif
    if (pMac->fEnableDebugLog & 0x1) {
        if ((fc.type == SIR_MAC_MGMT_FRAME) &&
                (fc.subType != SIR_MAC_MGMT_PROBE_REQ) &&
                (fc.subType != SIR_MAC_MGMT_PROBE_RSP) &&
                (fc.subType != SIR_MAC_MGMT_BEACON))
        {
            limLog(pMac, LOGE, FL("RX MGMT - Type %hu, SubType %hu"),
                    fc.type, fc.subType);
        }
    }
#ifdef FEATURE_WLAN_EXTSCAN
    if (WMA_IS_EXTSCAN_SCAN_SRC(pRxPacketInfo) ||
        WMA_IS_EPNO_SCAN_SRC(pRxPacketInfo)) {
        if (fc.subType == SIR_MAC_MGMT_BEACON ||
            fc.subType == SIR_MAC_MGMT_PROBE_RSP) {
            __limProcessExtScanBeaconProbeRsp(pMac, pRxPacketInfo, fc.subType);
        } else {
            limLog(pMac, LOGE, FL("Wrong frameType %d, Subtype %d for %d"),
                    fc.type, fc.subType, WMA_GET_SCAN_SRC(pRxPacketInfo));
        }
        goto end;
    }
#endif

#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
    if ( WDA_GET_ROAMCANDIDATEIND(pRxPacketInfo))
    {
        limLog(pMac, LOG2, FL("Notify SME with candidate ind"));
        limSendSmeCandidateFoundInd(pMac, WDA_GET_SESSIONID(pRxPacketInfo));
        goto end;
    }
    if (WDA_GET_OFFLOADSCANLEARN(pRxPacketInfo))
    {
        if (fc.subType == SIR_MAC_MGMT_BEACON)
        {
            limLog( pMac, LOG2, FL("Save this beacon in LFR cache"));
            __limHandleBeacon(pMac, limMsg, NULL);
        }
        else if (fc.subType == SIR_MAC_MGMT_PROBE_RSP)
        {
            limLog( pMac, LOG2, FL("Save this probe rsp in LFR cache"));
            limProcessProbeRspFrameNoSession(pMac, pRxPacketInfo);
        }
        else
        {
            limLog( pMac, LOGE, FL("Wrong frame Type %d, Subtype %d for LFR"),
                    fc.type, fc.subType);
        }
        goto end;
    }
#endif //WLAN_FEATURE_ROAM_SCAN_OFFLOAD
#if defined(FEATURE_WLAN_ESE) && !defined(FEATURE_WLAN_ESE_UPLOAD)
    if (fc.type == SIR_MAC_DATA_FRAME && isFrmFt)
    {
        limLog(pMac, LOGE,
               FL("Need to port handling of IAPP frames to QCACLD for ESE"));
    } else
#endif
    /* Added For BT-AMP Support */
    if((psessionEntry = peFindSessionByBssid(pMac,pHdr->bssId,&sessionId))== NULL)
    {
#ifdef WLAN_FEATURE_VOWIFI_11R
        if (fc.subType == SIR_MAC_MGMT_AUTH)
        {
#ifdef WLAN_FEATURE_VOWIFI_11R_DEBUG
            limLog( pMac, LOG1, FL("ProtVersion %d, Type %d, Subtype %d rateIndex=%d"),
                    fc.protVer, fc.type, fc.subType, WDA_GET_RX_MAC_RATE_IDX(pRxPacketInfo));
            limPrintMacAddr(pMac, pHdr->bssId, LOG1);
#endif
            if (limProcessAuthFrameNoSession(pMac, pRxPacketInfo, limMsg->bodyptr) == eSIR_SUCCESS)
            {
                goto end;
            }
        }
#endif
        if((fc.subType != SIR_MAC_MGMT_PROBE_RSP )&&
            (fc.subType != SIR_MAC_MGMT_BEACON)&&
            (fc.subType != SIR_MAC_MGMT_PROBE_REQ)
            && (fc.subType != SIR_MAC_MGMT_ACTION ) //Public action frame can be received from non-associated stations.
          )
        {

            if((psessionEntry = peFindSessionByPeerSta(pMac,pHdr->sa,&sessionId))== NULL)
            {
               limLog(pMac, LOG1, FL("session does not exist for given bssId"));
               goto end;
            }
            else
               limLog(pMac,LOG1,"SessionId:%d Session Exist for given Bssid",
                      psessionEntry->peSessionId);
        }
        //  For p2p resp frames search for valid session with DA as
        //  BSSID will be SA and session will be present with DA only
        if(fc.subType == SIR_MAC_MGMT_ACTION )
        {
           psessionEntry = peFindSessionByBssid(pMac,pHdr->da,&sessionId);
        }
    }


    /* Check if frame is registered by HDD */
    if(limCheckMgmtRegisteredFrames(pMac, pRxPacketInfo, psessionEntry))
    {
        limLog( pMac, LOG1, FL("Received frame is passed to SME"));
        goto end;
    }



    if (fc.protVer != SIR_MAC_PROTOCOL_VERSION)
    {   // Received Frame with non-zero Protocol Version
        limLog(pMac, LOGE, FL("Unexpected frame with protVersion %d received"),
           fc.protVer);
#ifdef WLAN_DEBUG
        pMac->lim.numProtErr++;
#endif
        goto end;
    }

    if (!pMac->fScanOffload)
    {
        if (limIsSystemInScanState(pMac))
        {
            limHandleFramesInScanState(pMac, limMsg, pRxPacketInfo, pDeferMsg, psessionEntry);
            return;
        }
    }

/* Chance of crashing : to be done BT-AMP ........happens when broadcast probe req is received */

#ifdef WLAN_DEBUG
    pMac->lim.numMAC[fc.type][fc.subType]++;
#endif

    switch (fc.type)
    {
        case SIR_MAC_MGMT_FRAME:
        {
            // Received Management frame
            switch (fc.subType)
            {
                case SIR_MAC_MGMT_ASSOC_REQ:
                    // Make sure the role supports Association
                    if (LIM_IS_BT_AMP_AP_ROLE(psessionEntry) ||
                        LIM_IS_AP_ROLE(psessionEntry))
                        limProcessAssocReqFrame(pMac, pRxPacketInfo, LIM_ASSOC, psessionEntry);
                    else {
                        // Unwanted messages - Log error
                        limLog(pMac, LOGE, FL("unexpected message received %X"),limMsg->type);
                    }
                    break;

                case SIR_MAC_MGMT_ASSOC_RSP:
                    limProcessAssocRspFrame(pMac, pRxPacketInfo, LIM_ASSOC,psessionEntry);
                    break;

                case SIR_MAC_MGMT_REASSOC_REQ:
                    // Make sure the role supports Reassociation
                    if (LIM_IS_BT_AMP_AP_ROLE(psessionEntry) ||
                        LIM_IS_AP_ROLE(psessionEntry)) {
                        limProcessAssocReqFrame(pMac, pRxPacketInfo, LIM_REASSOC, psessionEntry);
                    } else {
                        // Unwanted messages - Log error
                        limLog(pMac, LOGE, FL("unexpected message received %X"),limMsg->type);
                    }
                    break;

                case SIR_MAC_MGMT_REASSOC_RSP:
                    limProcessAssocRspFrame(pMac, pRxPacketInfo, LIM_REASSOC,psessionEntry);
                    break;

                case SIR_MAC_MGMT_PROBE_REQ:
                    limProcessProbeReqFrame_multiple_BSS(pMac, pRxPacketInfo,psessionEntry);
                    break;

                case SIR_MAC_MGMT_PROBE_RSP:
                    if(psessionEntry == NULL)
                        limProcessProbeRspFrameNoSession(pMac, pRxPacketInfo);
                    else
                        limProcessProbeRspFrame(pMac, pRxPacketInfo, psessionEntry);
                    break;

                case SIR_MAC_MGMT_BEACON:
                    __limHandleBeacon(pMac, limMsg,psessionEntry);
                    break;

                case SIR_MAC_MGMT_DISASSOC:
                    limProcessDisassocFrame(pMac, pRxPacketInfo,psessionEntry);
                    break;

                case SIR_MAC_MGMT_AUTH:
                    limProcessAuthFrame(pMac, pRxPacketInfo,psessionEntry);
                    break;

                case SIR_MAC_MGMT_DEAUTH:
                    limProcessDeauthFrame(pMac, pRxPacketInfo,psessionEntry);
                    break;

                case SIR_MAC_MGMT_ACTION:
                   if(psessionEntry == NULL)
                      limProcessActionFrameNoSession(pMac, pRxPacketInfo);
                   else
                   {
                      if (WDA_GET_RX_UNKNOWN_UCAST(pRxPacketInfo))
                         limHandleUnknownA2IndexFrames(pMac, pRxPacketInfo,psessionEntry);
                     else
                         limProcessActionFrame(pMac, pRxPacketInfo,psessionEntry);
                   }
                   break;
                default:
                    // Received Management frame of 'reserved' subtype
                    break;
            } // switch (fc.subType)

        }
        break;
        case SIR_MAC_DATA_FRAME:
        {
#if defined(FEATURE_WLAN_ESE) && !defined(FEATURE_WLAN_ESE_UPLOAD)
             /* We accept data frame (IAPP frame) only if Session is
              * present and ese connection is established on that
              * session
              */
             if (psessionEntry && psessionEntry->isESEconnection) {
                 limProcessIappFrame(pMac, pRxPacketInfo, psessionEntry);
             }
#endif
        }
        break;
        default:
            // Received frame of type 'reserved'
            break;

    } // switch (fc.type)

end:
    limPktFree(pMac, HAL_TXRX_FRM_802_11_MGMT, pRxPacketInfo, (void *) limMsg->bodyptr) ;
    return;
} /*** end limHandle80211Frames() ***/

/**
 * limSendStopScanOffloadReq()
 *
 *FUNCTION:
 * This function will be called to abort the ongoing offloaded scan
 * request.
 *
 *
 *NOTE:
 *
 * @param  pMac      Pointer to Global MAC structure
 * @return eHAL_STATUS_SUCCESS or eHAL_STATUS_FAILURE
 */
eHalStatus limSendStopScanOffloadReq(tpAniSirGlobal pMac, tANI_U8 SessionId)
{
    tSirMsgQ msg;
    tSirRetStatus rc = eSIR_SUCCESS;
    tAbortScanParams *pAbortScanParams;

    pAbortScanParams = vos_mem_malloc(sizeof(tAbortScanParams));
    if (NULL == pAbortScanParams)
    {
        limLog(pMac, LOGP, FL("Memory allocation failed for AbortScanParams"));
        return eHAL_STATUS_FAILURE;
    }

    pAbortScanParams->SessionId = SessionId;
    msg.type = WDA_STOP_SCAN_OFFLOAD_REQ;
    msg.bodyptr = pAbortScanParams;
    msg.bodyval = 0;

    rc = wdaPostCtrlMsg(pMac, &msg);
    if (rc != eSIR_SUCCESS)
    {
        limLog(pMac, LOGE, FL("wdaPostCtrlMsg() return failure"));
        vos_mem_free(pAbortScanParams);
        return eHAL_STATUS_FAILURE;
    }

    limLog(pMac, LOG1, FL("Abort ongoing offload scan."));
    return eHAL_STATUS_SUCCESS;

}

/**
 * limProcessAbortScanInd()
 *
 *FUNCTION:
 * This function is called from HDD to abort the scan which is presently being run
 *
 *
 *NOTE:
 *
 * @param  pMac      Pointer to Global MAC structure
 * @param  *pMsgBuf  A pointer to the SME message buffer
 * @return None
 */
void
limProcessAbortScanInd(tpAniSirGlobal pMac, tANI_U8 SessionId)
{
#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM //FEATURE_WLAN_DIAG_SUPPORT
    limDiagEventReport(pMac, WLAN_PE_DIAG_SCAN_ABORT_IND_EVENT, NULL, 0, 0);
#endif //FEATURE_WLAN_DIAG_SUPPORT

    /* Deactivate the gLimBackgroundScanTimer as part of the abort scan.
     * SME should send WNI_CFG_BACKGROUND_SCAN_PERIOD indication
     * to start the background scan again
     */
    PELOG2(limLog(pMac, LOG2, FL("Processing AbortScan Ind"));)

    limAbortBackgroundScan(pMac);

    if (pMac->fScanOffload)
    {
        /* send stop scan cmd to fw if scan offload is enabled. */
        limSendStopScanOffloadReq(pMac, SessionId);
    }
    else
    {
        /* Abort the scan if its running, else just return */
        if(limIsSystemInScanState(pMac))
        {
            if( (eLIM_HAL_INIT_SCAN_WAIT_STATE == pMac->lim.gLimHalScanState ) ||
                    (eLIM_HAL_START_SCAN_WAIT_STATE == pMac->lim.gLimHalScanState ) ||
                    (eLIM_HAL_END_SCAN_WAIT_STATE == pMac->lim.gLimHalScanState ) ||
                    (eLIM_HAL_FINISH_SCAN_WAIT_STATE == pMac->lim.gLimHalScanState) )
            {
                //Simply signal we need to abort
                limLog( pMac, LOGW, FL(" waiting for HAL, simply signal abort gLimHalScanState = %d"), pMac->lim.gLimHalScanState );
                pMac->lim.abortScan = 1;
            }
            else
            {
                //Force abort
                limLog( pMac, LOGW, FL(" Force aborting scan") );
                pMac->lim.abortScan = 0;
                limDeactivateAndChangeTimer(pMac, eLIM_MIN_CHANNEL_TIMER);
                limDeactivateAndChangeTimer(pMac, eLIM_MAX_CHANNEL_TIMER);
                //Set the resume channel to Any valid channel (invalid).
                //This will instruct HAL to set it to any previous valid channel.
                peSetResumeChannel(pMac, 0, 0);
                limSendHalFinishScanReq(pMac, eLIM_HAL_FINISH_SCAN_WAIT_STATE);
            }
        }
    }
    return;
}

/**
 * limMessageProcessor
 *
 *FUNCTION:
 * Wrapper function for limProcessMessages when handling messages received by LIM.
 * Could either defer messages or process them.
 * @param  pMac   Pointer to Global MAC structure
 * @param  limMsg Received LIM message
 * @return None
 */

void limMessageProcessor(tpAniSirGlobal pMac, tpSirMsgQ limMsg)
{
    if (eLIM_MLM_OFFLINE_STATE == pMac->lim.gLimMlmState)
    {
        peFreeMsg(pMac, limMsg);
        return;
    }

    if (!defMsgDecision(pMac, limMsg))
    {
        limProcessMessages(pMac, limMsg);
        // process deferred message queue if allowed
        {
            if ( (! (pMac->lim.gLimAddtsSent))
                            &&
                    (! (limIsSystemInScanState(pMac)))
                )
            {
                if (true == GET_LIM_PROCESS_DEFD_MESGS(pMac))
                  limProcessDeferredMessageQueue(pMac);
            }
        }
    }
}

#ifdef FEATURE_OEM_DATA_SUPPORT

void limOemDataRspHandleResumeLinkRsp(tpAniSirGlobal pMac, eHalStatus status, tANI_U32* mlmOemDataRsp)
{
    if(status != eHAL_STATUS_SUCCESS)
    {
        limLog(pMac, LOGE, FL("OEM Data Rsp failed to get the response for resume link"));
    }

    if(NULL != pMac->lim.gpLimMlmOemDataReq)
    {
        vos_mem_free(pMac->lim.gpLimMlmOemDataReq);
        pMac->lim.gpLimMlmOemDataReq = NULL;
    }

    //"Failure" status doesn't mean that Oem Data Rsp did not happen
    //and hence we need to respond to upper layers. Only Resume link is failed, but
    //we got the oem data response already.
    //Post the meessage to MLM
    limPostSmeMessage(pMac, LIM_MLM_OEM_DATA_CNF, (tANI_U32*)(mlmOemDataRsp));

    return;
}

void limProcessOemDataRsp(tpAniSirGlobal pMac, tANI_U32* body)
{
    tpLimMlmOemDataRsp mlmOemDataRsp = NULL;

    //Process all the messages for the lim queue
    SET_LIM_PROCESS_DEFD_MESGS(pMac, true);

    mlmOemDataRsp = (tpLimMlmOemDataRsp) body;

    PELOG1(limLog(pMac, LOG1, FL("%s: sending oem data response msg to sme"),
                  __func__);)
    limPostSmeMessage(pMac, LIM_MLM_OEM_DATA_CNF, (tANI_U32*)(mlmOemDataRsp));

    return;
}

#endif

/**
 * limProcessMessages
 *
 *FUNCTION:
 * This function is called by limProcessMessageQueue function. This
 * function processes messages received by LIM.
 *
 *LOGIC:
 * Depending on the message type, corresponding function will be
 * called, for example limProcessSmeMessages() will be called to
 * process SME messages received from HDD/Upper layer software module.
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pMac   Pointer to Global MAC structure
 * @param  limMsg Received LIM message
 * @return None
 */

void
limProcessMessages(tpAniSirGlobal pMac, tpSirMsgQ  limMsg)
{
#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
    uint8_t vdev_id = 0;
    uint8_t i;
    tpPESession session_entry = NULL;
    tUpdateBeaconParams beacon_params;
#endif /* FEATURE_AP_MCC_CH_AVOIDANCE */

    tANI_U8  deferMsg = false;
    tLinkStateParams *linkStateParams;
#if defined WLAN_FEATURE_VOWIFI_11R
    tpPESession pSession;
#endif
    if (ANI_DRIVER_TYPE(pMac) == eDRIVER_TYPE_MFG) {
        vos_mem_free(limMsg->bodyptr);
        limMsg->bodyptr = NULL;
        return;
    }
    if (limMsg == NULL)
    {
        limLog(pMac, LOGE, FL("Message pointer is Null"));
        VOS_ASSERT(0);
        return;
    }
#ifdef WLAN_DEBUG
    pMac->lim.numTot++;
#endif

    MTRACE(macTraceMsgRx(pMac, NO_SESSION, LIM_TRACE_MAKE_RXMSG(limMsg->type, LIM_MSG_PROCESSED));)

    switch (limMsg->type)
    {

        case SIR_LIM_UPDATE_BEACON:
            limUpdateBeacon(pMac);
            break;

        case SIR_CFG_PARAM_UPDATE_IND:
            /// CFG parameter updated
            if (limIsSystemInScanState(pMac))
            {
                // System is in DFS (Learn) mode
                // Defer processsing this message
                if (limDeferMsg(pMac, limMsg) != TX_SUCCESS)
                {
                    if(!(pMac->lim.deferredMsgCnt & 0xF))
                    {
                        PELOGE(limLog(pMac, LOGE, FL("Unable to Defer message(0x%X) limSmeState %d (prev sme state %d) sysRole %d mlm state %d (prev mlm state %d)"),
                        limMsg->type, pMac->lim.gLimSmeState,  pMac->lim.gLimPrevSmeState,
                        pMac->lim.gLimSystemRole,  pMac->lim.gLimMlmState,  pMac->lim.gLimPrevMlmState);)
                    }
                    limLogSessionStates(pMac);
                }
            }
            else
            {
                limHandleCFGparamUpdate(pMac, limMsg->bodyval);
            }

            break;

        case WDA_INIT_SCAN_RSP:
            limProcessInitScanRsp(pMac, limMsg->bodyptr);
            limMsg->bodyptr = NULL;
            break;

        case WDA_START_SCAN_RSP:
            limProcessStartScanRsp(pMac, limMsg->bodyptr);
            limMsg->bodyptr = NULL;
            break;

        case WDA_END_SCAN_RSP:
            limProcessEndScanRsp(pMac, limMsg->bodyptr);
            limMsg->bodyptr = NULL;
            break;

        case WDA_FINISH_SCAN_RSP:
            limProcessFinishScanRsp(pMac, limMsg->bodyptr);
            limMsg->bodyptr = NULL;
            break;
#ifdef FEATURE_OEM_DATA_SUPPORT
        case WDA_START_OEM_DATA_RSP:
            limProcessOemDataRsp(pMac, limMsg->bodyptr);
            limMsg->bodyptr = NULL;
            break;
#endif

        case WDA_SWITCH_CHANNEL_RSP:
            limProcessSwitchChannelRsp(pMac, limMsg->bodyptr);
            limMsg->bodyptr = NULL;
            break;

#ifdef ANI_SIR_IBSS_PEER_CACHING
        case WDA_IBSS_STA_ADD:
            limIbssStaAdd(pMac, limMsg->bodyptr);
            break;
#endif
        case SIR_BB_XPORT_MGMT_MSG:
            // These messages are from Peer MAC entity.
#ifdef WLAN_DEBUG
            pMac->lim.numBbt++;
#endif

            {
                v_U16_t     pktLen = 0;
                vos_pkt_t  *pVosPkt;
                VOS_STATUS  vosStatus;
                tSirMsgQ    limMsgNew;

                /* The original limMsg which we were deferring have the
                 * bodyPointer point to 'BD' instead of 'Vos pkt'. If we don't make a copy
                 * of limMsg, then vos_pkt_peek_data will overwrite the limMsg->bodyPointer.
                 * and next time when we try to process the msg, we will try to use 'BD' as
                 * 'Vos Pkt' which will cause a crash
                 */
                if (limMsg->bodyptr == NULL)
                {
                    limLog(pMac, LOGE, FL("Message bodyptr is Null"));
                    VOS_ASSERT(0);
                    break;
                }
                vos_mem_copy((tANI_U8*)&limMsgNew, (tANI_U8*)limMsg,
                             sizeof(tSirMsgQ));
                pVosPkt = (vos_pkt_t *)limMsgNew.bodyptr;
                vos_pkt_get_packet_length(pVosPkt, &pktLen);

                vosStatus = WDA_DS_PeekRxPacketInfo( pVosPkt, (v_PVOID_t *)&limMsgNew.bodyptr, VOS_FALSE );

                if( !VOS_IS_STATUS_SUCCESS(vosStatus) )
                {
                    vos_pkt_return_packet(pVosPkt);
                    break;

                }

#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
                if (WDA_GET_ROAMCANDIDATEIND(limMsgNew.bodyptr))
                    limLog(pMac, LOG1, FL("roamCandidateInd %d"),
                           WDA_GET_ROAMCANDIDATEIND(limMsgNew.bodyptr));

                if (WDA_GET_OFFLOADSCANLEARN(limMsgNew.bodyptr))
                    limLog(pMac, LOG1, FL("offloadScanLearn %d"),
                           WDA_GET_OFFLOADSCANLEARN(limMsgNew.bodyptr));
#endif

                limHandle80211Frames(pMac, &limMsgNew, &deferMsg);

                if ( deferMsg == true )
                {
                    PELOG1(limLog(pMac, LOG1, FL("Defer message type=%X "), limMsg->type);)
                        if (limDeferMsg(pMac, limMsg) != TX_SUCCESS)
                        {
                            PELOGE(limLog(pMac, LOGE, FL("Unable to Defer message(0x%X) limSmeState %d (prev sme state %d) sysRole %d mlm state %d (prev mlm state %d)"),
                                limMsg->type, pMac->lim.gLimSmeState,  pMac->lim.gLimPrevSmeState,
                                pMac->lim.gLimSystemRole,  pMac->lim.gLimMlmState,  pMac->lim.gLimPrevMlmState);)
                            limLogSessionStates(pMac);
                            vos_pkt_return_packet(pVosPkt);
                        }
                }
                else
                {
                    /* PE is not deferring this 802.11 frame so we need to call vos_pkt_return.
                     * Asumption here is when Rx mgmt frame processing is done,
                     * voss packet could be freed here.
                     */
                    vos_pkt_return_packet(pVosPkt);
                }
            }
            break;

        case eWNI_SME_SCAN_REQ:
        case eWNI_SME_REMAIN_ON_CHANNEL_REQ:
        case eWNI_SME_DISASSOC_REQ:
        case eWNI_SME_DEAUTH_REQ:
        case eWNI_SME_GET_SCANNED_CHANNEL_REQ:
#ifdef FEATURE_OEM_DATA_SUPPORT
        case eWNI_SME_OEM_DATA_REQ:
#endif
#ifdef FEATURE_WLAN_TDLS
        case eWNI_SME_TDLS_SEND_MGMT_REQ:
        case eWNI_SME_TDLS_ADD_STA_REQ:
        case eWNI_SME_TDLS_DEL_STA_REQ:
        case eWNI_SME_TDLS_LINK_ESTABLISH_REQ:
#endif
        case eWNI_SME_RESET_AP_CAPS_CHANGED:
            // These messages are from HDD
            limProcessNormalHddMsg(pMac, limMsg, true);  //need to response to hdd
            break;

        case eWNI_SME_SCAN_ABORT_IND:
          {
            tSirMbMsg *pMsg = limMsg->bodyptr;
            tANI_U8 sessionId;
            if (pMsg)
            {
               sessionId = (tANI_U8) pMsg->data[0];
               limProcessAbortScanInd(pMac, sessionId);
               vos_mem_free((v_VOID_t *)limMsg->bodyptr);
               limMsg->bodyptr = NULL;
            }
          }
          break;
        case eWNI_SME_PDEV_SET_HT_VHT_IE:
        case eWNI_SME_START_REQ:
        case eWNI_SME_SYS_READY_IND:
        case eWNI_SME_JOIN_REQ:
        case eWNI_SME_REASSOC_REQ:
        case eWNI_SME_START_BSS_REQ:
        case eWNI_SME_STOP_BSS_REQ:
        case eWNI_SME_SWITCH_CHL_REQ:
        case eWNI_SME_SWITCH_CHL_CB_PRIMARY_REQ:
        case eWNI_SME_SWITCH_CHL_CB_SECONDARY_REQ:
        case eWNI_SME_SETCONTEXT_REQ:
        case eWNI_SME_REMOVEKEY_REQ:
        case eWNI_SME_DISASSOC_CNF:
        case eWNI_SME_DEAUTH_CNF:
        case eWNI_SME_ASSOC_CNF:
        case eWNI_SME_REASSOC_CNF:
        case eWNI_SME_ADDTS_REQ:
        case eWNI_SME_DELTS_REQ:
        case eWNI_SME_DEL_BA_PEER_IND:
        case eWNI_SME_SET_TX_POWER_REQ:
        case eWNI_SME_GET_TX_POWER_REQ:
        case eWNI_SME_GET_NOISE_REQ:
        case eWNI_SME_GET_ASSOC_STAS_REQ:
        case eWNI_SME_TKIP_CNTR_MEAS_REQ:
        case eWNI_SME_UPDATE_APWPSIE_REQ:
        case eWNI_SME_HIDE_SSID_REQ:
        case eWNI_SME_GET_WPSPBC_SESSION_REQ:
        case eWNI_SME_SET_APWPARSNIEs_REQ:
        case eWNI_SME_CHNG_MCC_BEACON_INTERVAL:
#if defined WLAN_FEATURE_VOWIFI
        case eWNI_SME_NEIGHBOR_REPORT_REQ_IND:
        case eWNI_SME_BEACON_REPORT_RESP_XMIT_IND:
#endif
#if defined FEATURE_WLAN_ESE
        case eWNI_SME_ESE_ADJACENT_AP_REPORT:
#endif
#ifdef WLAN_FEATURE_VOWIFI_11R
        case eWNI_SME_FT_UPDATE_KEY:
        case eWNI_SME_FT_PRE_AUTH_REQ:
        case eWNI_SME_FT_AGGR_QOS_REQ:
#endif
        case eWNI_SME_ADD_STA_SELF_REQ:
        case eWNI_SME_DEL_STA_SELF_REQ:
        case eWNI_SME_REGISTER_MGMT_FRAME_REQ:
        case eWNI_SME_UPDATE_NOA:
        case eWNI_SME_CLEAR_DFS_CHANNEL_LIST:
        case eWNI_SME_CLEAR_LIM_SCAN_CACHE:
        case eWNI_SME_STA_STAT_REQ:
        case eWNI_SME_AGGR_STAT_REQ:
        case eWNI_SME_GLOBAL_STAT_REQ:
        case eWNI_SME_STAT_SUMM_REQ:
        case eWNI_SME_GET_STATISTICS_REQ:
#if defined(FEATURE_WLAN_ESE) && defined(FEATURE_WLAN_ESE_UPLOAD)
        case eWNI_SME_GET_TSM_STATS_REQ:
#endif /* FEATURE_WLAN_ESE && FEATURE_WLAN_ESE_UPLOAD */
        case eWNI_SME_EXT_CHANGE_CHANNEL:
            // These messages are from HDD
            limProcessNormalHddMsg(pMac, limMsg, false);   //no need to response to hdd
            break;

        //Power Save Messages From HDD
        case eWNI_PMC_PWR_SAVE_CFG:
        case eWNI_PMC_ENTER_BMPS_REQ:
        case eWNI_PMC_EXIT_BMPS_REQ:
        case eWNI_PMC_ENTER_IMPS_REQ:
        case eWNI_PMC_EXIT_IMPS_REQ:
        case eWNI_PMC_ENTER_UAPSD_REQ:
        case eWNI_PMC_EXIT_UAPSD_REQ:
        case eWNI_PMC_ENTER_WOWL_REQ:
        case eWNI_PMC_EXIT_WOWL_REQ:
        case eWNI_PMC_WOWL_ADD_BCAST_PTRN:
        case eWNI_PMC_WOWL_DEL_BCAST_PTRN:
            if(!pMac->psOffloadEnabled)
                pmmProcessMessage(pMac, limMsg);
            else
                pmmOffloadProcessMessage(pMac, limMsg);
            break;

        case eWNI_PMC_SMPS_STATE_IND :
        {
            if(limMsg->bodyptr){
            vos_mem_free(limMsg->bodyptr);
            limMsg->bodyptr = NULL;
            }
        }
            break;
        case eWNI_SME_SEND_ACTION_FRAME_IND:
            limSendP2PActionFrame(pMac, limMsg);
            vos_mem_free(limMsg->bodyptr);
            limMsg->bodyptr = NULL;
            break;
        case eWNI_SME_ABORT_REMAIN_ON_CHAN_IND:
        {
            tSirMbMsgP2p *pMbMsg = (tSirMbMsgP2p *)limMsg->bodyptr;
            limAbortRemainOnChan(pMac, pMbMsg->sessionId);
            vos_mem_free(limMsg->bodyptr);
            limMsg->bodyptr = NULL;
            break;
        }

        case SIR_HAL_P2P_NOA_START_IND:
        {
            tpPESession psessionEntry = &pMac->lim.gpSession[0];
            tANI_U8  i;
            tANI_U8 p2pGOExists = 0;

            limLog(pMac, LOG1, "LIM received NOA start %x", limMsg->type);

            /* Since insert NOA is done and NOA start msg received, we should deactivate the Insert NOA timer */
            limDeactivateAndChangeTimer(pMac, eLIM_INSERT_SINGLESHOT_NOA_TIMER);

            for(i=0; i < pMac->lim.maxBssId; i++)
            {
                psessionEntry = &pMac->lim.gpSession[i];
                if   ( (psessionEntry != NULL) && (psessionEntry->valid) &&
                    (psessionEntry->pePersona == VOS_P2P_GO_MODE))
                { //Save P2P NOA start attributes for P2P Go persona
                    p2pGOExists = 1;
                    vos_mem_copy(&psessionEntry->p2pGoPsNoaStartInd, limMsg->bodyptr,
                                 sizeof(tSirP2PNoaStart));
                    if (psessionEntry->p2pGoPsNoaStartInd.status != eHAL_STATUS_SUCCESS)
                    {
                        limLog(pMac, LOGW, FL("GO NOA start failure status %d reported by FW."
                            " - still go ahead with deferred sme req. This is just info"),
                            psessionEntry->p2pGoPsNoaStartInd.status);
                    }
                    break;
                }
            }

            if (p2pGOExists == 0)
            {
                limLog(pMac, LOGW, FL("By the time, we received NOA start, GO is already removed."
                        " - still go ahead with deferred sme req. This is just info"));
            }

            /* We received the NOA start indication. Now we can send down the SME request which requires off-channel operation */
            limProcessRegdDefdSmeReqAfterNOAStart(pMac);
            vos_mem_free(limMsg->bodyptr);
            limMsg->bodyptr = NULL;
         }
            break;
#ifdef FEATURE_WLAN_TDLS
        case SIR_HAL_TDLS_IND:
        {
            tSirTdlsInd  *pTdlsInd = (tpSirTdlsInd)limMsg->bodyptr ;
            tpDphHashNode pStaDs = NULL ;
            tpPESession psessionEntry = NULL;
            tANI_U8             sessionId;
            if((psessionEntry = peFindSessionByStaId(pMac,pTdlsInd->staIdx,&sessionId))== NULL)
            {
               limLog(pMac, LOG1, FL("session does not exist for given bssId"));
               vos_mem_free(limMsg->bodyptr);
               limMsg->bodyptr = NULL;
               return;
            }
            if ((pStaDs = dphGetHashEntry(pMac, pTdlsInd->assocId, &psessionEntry->dph.dphHashTable)) == NULL)
            {
               limLog(pMac, LOG1, FL("pStaDs Does not exist for given staId"));
               vos_mem_free(limMsg->bodyptr);
               limMsg->bodyptr = NULL;
               return;
            }

            if ((STA_ENTRY_TDLS_PEER == pStaDs->staType))
            {
                limLog(pMac, LOGE,
                       FL("received TDLS Indication from the Firmware with Reason Code %d "),
                       pTdlsInd->reasonCode);
                limSendSmeTDLSDelStaInd(pMac, pStaDs, psessionEntry,
                                        pTdlsInd->reasonCode);
            }
            vos_mem_free(limMsg->bodyptr);
            limMsg->bodyptr = NULL;
         }
         break;
#endif
        case SIR_HAL_P2P_NOA_ATTR_IND:
            {
                tpPESession psessionEntry = &pMac->lim.gpSession[0];
                tANI_U8  i;

                limLog(pMac, LOG1, FL("Received message Noa_ATTR %x"), limMsg->type);
                for(i=0; i < pMac->lim.maxBssId; i++)
                {
                    psessionEntry = &pMac->lim.gpSession[i];
                    if   ( (psessionEntry != NULL) && (psessionEntry->valid) &&
                        (psessionEntry->pePersona == VOS_P2P_GO_MODE))
                    { //Save P2P attributes for P2P Go persona

                        vos_mem_copy(&psessionEntry->p2pGoPsUpdate, limMsg->bodyptr,
                                     sizeof(tSirP2PNoaAttr));

                        limLog(pMac, LOG2, FL(" &psessionEntry->bssId "
                                     MAC_ADDRESS_STR " ctWin=%d oppPsFlag=%d"),
                                     MAC_ADDR_ARRAY(psessionEntry->bssId),
                                     psessionEntry->p2pGoPsUpdate.ctWin,
                                     psessionEntry->p2pGoPsUpdate.oppPsFlag);

                        limLog(pMac, LOG2, FL(" uNoa1IntervalCnt=%d uNoa1Duration=%d uNoa1Interval=%d uNoa1StartTime=%d"),
                                     psessionEntry->p2pGoPsUpdate.uNoa1IntervalCnt,
                                     psessionEntry->p2pGoPsUpdate.uNoa1Duration,
                                     psessionEntry->p2pGoPsUpdate.uNoa1Interval,
                                     psessionEntry->p2pGoPsUpdate.uNoa1StartTime);


                        break;
                    }
                }

            }
            vos_mem_free(limMsg->bodyptr);
            limMsg->bodyptr = NULL;

            break;


        /* eWNI_SME_PRE_CHANNEL_SWITCH_FULL_POWER Message comes after the
         * device comes out of full power for the full power request sent
         * because of channel switch with switch count as 0, so call the same
         * function used in timeout case(i.e SIR_LIM_CHANNEL_SWITCH_TIMEOUT)
         * for switching the channel*/
        case eWNI_SME_PRE_CHANNEL_SWITCH_FULL_POWER:
            if ( !tx_timer_running(&pMac->lim.limTimers.gLimChannelSwitchTimer) )
            {
                limProcessChannelSwitchTimeout(pMac);
            }
            vos_mem_free(limMsg->bodyptr);
            limMsg->bodyptr = NULL;
            break;

        //Power Save Related Messages From HAL
        case WDA_ENTER_BMPS_RSP:
        case WDA_EXIT_BMPS_RSP:
        case WDA_EXIT_BMPS_IND:
        case WDA_ENTER_IMPS_RSP:
        case WDA_EXIT_IMPS_RSP:
        case WDA_ENTER_UAPSD_RSP:
        case WDA_EXIT_UAPSD_RSP:
        case WDA_WOWL_ENTER_RSP:
        case WDA_WOWL_EXIT_RSP:
            if(!pMac->psOffloadEnabled)
                pmmProcessMessage(pMac, limMsg);
            else
                pmmOffloadProcessMessage(pMac, limMsg);
            break;

        case WDA_MISSED_BEACON_IND:
            if(pMac->psOffloadEnabled)
                limPsOffloadHandleMissedBeaconInd(pMac, limMsg);
            else
                limHandleMissedBeaconInd(pMac, limMsg);

            vos_mem_free(limMsg->bodyptr);
            limMsg->bodyptr = NULL;
            break;
        case WDA_MIC_FAILURE_IND:
           limMicFailureInd(pMac, limMsg);
           vos_mem_free(limMsg->bodyptr);
           limMsg->bodyptr = NULL;
           break;
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
        case WDA_ROAM_OFFLOAD_SYNCH_IND:
           limRoamOffloadSynchInd(pMac, limMsg);
           /* bodyPtr is freed after handling
            * eWNI_SME_ROAM_OFFLOAD_SYNCH_IND in sme_ProcessMsg */
           break;
#endif



        case SIR_LIM_ADDTS_RSP_TIMEOUT:
            limProcessSmeReqMessages(pMac,limMsg);
            break;
#ifdef FEATURE_WLAN_ESE
        case SIR_LIM_ESE_TSM_TIMEOUT:
#ifndef FEATURE_WLAN_ESE_UPLOAD
            limProcessTsmTimeoutHandler(pMac,limMsg);
#endif /* FEATURE_WLAN_ESE_UPLOAD */
            break;
        case WDA_TSM_STATS_RSP:
#ifdef FEATURE_WLAN_ESE_UPLOAD
            limSendSmePEEseTsmRsp(pMac, (tAniGetTsmStatsRsp *)limMsg->bodyptr);
#else
            limProcessHalEseTsmRsp(pMac, limMsg);
#endif /* FEATURE_WLAN_ESE_UPLOAD */
            break;
#endif
        case WDA_ADD_TS_RSP:
            limProcessHalAddTsRsp(pMac, limMsg);
            break;

        case SIR_LIM_DEL_TS_IND:
          limProcessDelTsInd(pMac, limMsg);
            break;
        case SIR_LIM_ADD_BA_IND:
            limProcessAddBaInd(pMac, limMsg);
            break;
        case SIR_LIM_DEL_BA_ALL_IND:
            limDelAllBASessions(pMac);
            break;
        case SIR_LIM_DEL_BA_IND:
            limProcessMlmHalBADeleteInd( pMac, limMsg );
            break;

         case SIR_LIM_BEACON_GEN_IND: {

                if( pMac->lim.gLimSystemRole != eLIM_AP_ROLE )
                    schProcessPreBeaconInd(pMac, limMsg);

                }
                break;

        case SIR_LIM_DELETE_STA_CONTEXT_IND:
            limDeleteStaContext(pMac, limMsg);
            break;

        case SIR_LIM_MIN_CHANNEL_TIMEOUT:
        case SIR_LIM_MAX_CHANNEL_TIMEOUT:
        case SIR_LIM_PERIODIC_PROBE_REQ_TIMEOUT:
        case SIR_LIM_JOIN_FAIL_TIMEOUT:
        case SIR_LIM_PERIODIC_JOIN_PROBE_REQ_TIMEOUT:
        case SIR_LIM_AUTH_FAIL_TIMEOUT:
        case SIR_LIM_AUTH_RSP_TIMEOUT:
        case SIR_LIM_ASSOC_FAIL_TIMEOUT:
        case SIR_LIM_REASSOC_FAIL_TIMEOUT:
#ifdef WLAN_FEATURE_VOWIFI_11R
        case SIR_LIM_FT_PREAUTH_RSP_TIMEOUT:
#endif
        case SIR_LIM_REMAIN_CHN_TIMEOUT:
        case SIR_LIM_INSERT_SINGLESHOT_NOA_TIMEOUT:
        case SIR_LIM_DISASSOC_ACK_TIMEOUT:
        case SIR_LIM_DEAUTH_ACK_TIMEOUT:
        case SIR_LIM_CONVERT_ACTIVE_CHANNEL_TO_PASSIVE:
        case SIR_LIM_AUTH_RETRY_TIMEOUT:
            // These timeout messages are handled by MLM sub module

            limProcessMlmReqMessages(pMac,
                                     limMsg);

            break;

        case SIR_LIM_HEART_BEAT_TIMEOUT:
            /** check if heart beat failed, even if one Beacon
                    * is rcvd within the Heart Beat interval continue
                  * normal processing
                    */

            if(pMac->psOffloadEnabled)
            {
                /* Powersave Offload Case */
                /* TODO: Handle in Scan Case */
                /* TODO: handle in TDLS Case */
                if(NULL == limMsg->bodyptr)
                {
                    limLog(pMac, LOGE,
                    FL("Cannot Process HearBeat Timeout - bodyptr is Null"));
                }
                else
                {
                    tpPESession psessionEntry = (tpPESession)limMsg->bodyptr;
                    limLog(pMac, LOGE,
                    FL("Processing SIR_LIM_HEART_BEAT_TIMEOUT for Session %d"),
                    ((tpPESession)limMsg->bodyptr)->peSessionId);
                    limResetHBPktCount(psessionEntry);
                    limHandleHeartBeatTimeoutForSession(pMac, psessionEntry);
                }
                break;
            }
            if (limIsSystemInScanState(pMac))
            {
                // System is in DFS (Learn) mode
                // Defer processsing this message
                if (limDeferMsg(pMac, limMsg) != TX_SUCCESS)
                {
                    PELOGE(limLog(pMac, LOGE, FL("Unable to Defer message(0x%X) limSmeState %d (prev sme state %d) sysRole %d mlm state %d (prev mlm state %d)"),
                        limMsg->type, pMac->lim.gLimSmeState,  pMac->lim.gLimPrevSmeState,
                        pMac->lim.gLimSystemRole,  pMac->lim.gLimMlmState,  pMac->lim.gLimPrevMlmState);)
                    limLogSessionStates(pMac);
                }
            }
            else
            {
                 if (NULL == limMsg->bodyptr)
                 {
                     limHandleHeartBeatTimeout(pMac);
                 }
                 else
                 {
                     limHandleHeartBeatTimeoutForSession(pMac, (tpPESession)limMsg->bodyptr);
                 }
            }
            break;

        case SIR_LIM_PROBE_HB_FAILURE_TIMEOUT:
            limHandleHeartBeatFailureTimeout(pMac);
            break;

        case SIR_LIM_CHANNEL_SCAN_TIMEOUT:
            /**
             * Background scan timeout occurred on STA.
             * This is handled by LMM sub module.
             */
             limDeactivateAndChangeTimer(pMac, eLIM_BACKGROUND_SCAN_TIMER);

            //We will do background scan even in bcnps mode
            //if (pMac->sys.gSysEnableScanMode)
            pMac->lim.gLimReportBackgroundScanResults = FALSE;
            limTriggerBackgroundScan(pMac);
            break;


        case SIR_LIM_HASH_MISS_THRES_TIMEOUT:

            /*
            ** clear the credit to the send disassociate frame bucket
            **/

            pMac->lim.gLimDisassocFrameCredit = 0;
            break;

        case SIR_LIM_CNF_WAIT_TIMEOUT:

            /*
            ** Does not receive CNF or dummy packet
            **/
            limHandleCnfWaitTimeout(pMac, (tANI_U16) limMsg->bodyval);

            break;

        case SIR_LIM_KEEPALIVE_TIMEOUT:
            limSendKeepAliveToPeer(pMac);

            break;

        case SIR_LIM_RETRY_INTERRUPT_MSG:
            // Message from ISR upon TFP's max retry limit interrupt

            break;

        case SIR_LIM_INV_KEY_INTERRUPT_MSG:
            // Message from ISR upon SP's Invalid session key interrupt

            break;

        case SIR_LIM_KEY_ID_INTERRUPT_MSG:
            // Message from ISR upon SP's Invalid key ID interrupt

            break;

        case SIR_LIM_REPLAY_THRES_INTERRUPT_MSG:
            // Message from ISR upon SP's Replay threshold interrupt

            break;

        case SIR_LIM_CHANNEL_SWITCH_TIMEOUT:
            limProcessChannelSwitchTimeout(pMac);
            break;

        case SIR_LIM_QUIET_TIMEOUT:
            limProcessQuietTimeout(pMac);
            break;

        case SIR_LIM_QUIET_BSS_TIMEOUT:
            limProcessQuietBssTimeout(pMac);
            break;

        case SIR_LIM_UPDATE_OLBC_CACHEL_TIMEOUT:
            limHandleUpdateOlbcCache(pMac);
            break;


#ifdef FEATURE_WLAN_TDLS
        case SIR_HAL_TDLS_SHOULD_DISCOVER:
        case SIR_HAL_TDLS_SHOULD_TEARDOWN:
        case SIR_HAL_TDLS_PEER_DISCONNECTED:
            VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_INFO,
                      ("%s received tdls event: 0x%x"),
                      __func__, limMsg->type);
            limSendSmeTdlsEventNotify(pMac, limMsg->type,
                                      (void *)limMsg->bodyptr);
            break;
#endif

        case WDA_ADD_BSS_RSP:
            limProcessMlmAddBssRsp( pMac, limMsg );
            break;

        case WDA_ADD_STA_RSP:

            //call a wrapper by paasing bodyptr, their get sessionID and and call proper function from there.
            limProcessAddStaRsp(pMac,limMsg);
            break;

        case WDA_DELETE_STA_RSP:
            limProcessMlmDelStaRsp(pMac, limMsg);
            break;

        case WDA_ADD_STA_SELF_RSP:
            limProcessAddStaSelfRsp(pMac, limMsg);
            break;
        case WDA_DEL_STA_SELF_RSP:
            limProcessDelStaSelfRsp(pMac, limMsg);
            break;
#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
        case WDA_UPDATE_Q2Q_IE_IND:
            /*
             * this device is operating AP in MCC mode, update Q2Q IE in
             * beacon template
             */
             vos_mem_zero(&beacon_params, sizeof(tUpdateBeaconParams));
             beacon_params.paramChangeBitmap = 0;
             for ( i = 0; i < pMac->lim.maxBssId; i++) {
                 vdev_id = ((tANI_U8*)limMsg->bodyptr)[i];
                 session_entry = pe_find_session_by_sme_session_id(pMac,
                                                                vdev_id);
                 if(session_entry == NULL)
                     continue;
                 session_entry->sap_advertise_avoid_ch_ie =
                                            (tANI_U8)limMsg->bodyval;

                 /*
                  * if message comes for DFS channel, no need to update as:
                  * 1) We wont have MCC with DFS channels. so no need to add
                  *    Q2Q IE
                  * 2) We cannot end up in DFS channel SCC by channel switch
                  *    from non DFS MCC scenario, so no need to remove Q2Q IE
                  * 3) There is however a case where device start MCC and then
                  *    user modifies hostapd.conf and does SAP restart, in such
                  *    a case, beacon params will be reset and thus will not
                  *    contain Q2Q IE, by default.
                  */
                 if (vos_nv_getChannelEnabledState(
                         session_entry->currentOperChannel) != NV_CHANNEL_DFS) {
                     beacon_params.bssIdx = session_entry->bssIdx;
                     beacon_params.beaconInterval =
                         session_entry->beaconParams.beaconInterval;
                     beacon_params.paramChangeBitmap |=
                         PARAM_BCN_INTERVAL_CHANGED;
                     schSetFixedBeaconFields(pMac, session_entry);
                     limSendBeaconParams(pMac, &beacon_params, session_entry);
                 }
             }
             vos_mem_free(limMsg->bodyptr);
             break;
#endif /* FEATURE_AP_MCC_CH_AVOIDANCE */
        case WDA_DELETE_BSS_RSP:
            limHandleDeleteBssRsp(pMac,limMsg); //wrapper routine to handle delete bss response
            break;

        case WDA_CSA_OFFLOAD_EVENT:
            limHandleCSAoffloadMsg(pMac, limMsg);
            break;

        case WDA_SET_BSSKEY_RSP:
        case WDA_SET_STA_BCASTKEY_RSP:
            limProcessMlmSetBssKeyRsp( pMac, limMsg );
            break;
        case WDA_SET_STAKEY_RSP:
            limProcessMlmSetStaKeyRsp( pMac, limMsg );
            break;
        case WDA_REMOVE_BSSKEY_RSP:
        case WDA_REMOVE_STAKEY_RSP:
            limProcessMlmRemoveKeyRsp( pMac, limMsg );
            break;
        case WDA_ADDBA_RSP:
            limProcessMlmHalAddBARsp( pMac, limMsg );
            break;

        case WDA_STA_STAT_RSP:
        case WDA_AGGR_STAT_RSP:
        case WDA_GLOBAL_STAT_RSP:
        case WDA_STAT_SUMM_RSP:
            limSendSmeStatsRsp ( pMac, limMsg->type, (void *)limMsg->bodyptr);
            break;

        case WDA_GET_STATISTICS_RSP:
            limSendSmePEStatisticsRsp ( pMac, limMsg->type, (void *)limMsg->bodyptr);
            break;

        case WDA_SET_MIMOPS_RSP:
        case WDA_SET_TX_POWER_RSP:
            vos_mem_free((v_VOID_t*)limMsg->bodyptr);
            limMsg->bodyptr = NULL;
            break;

        case WDA_SET_MAX_TX_POWER_RSP:
#if defined WLAN_FEATURE_VOWIFI
            rrmSetMaxTxPowerRsp( pMac, limMsg );
#endif
            if(limMsg->bodyptr != NULL)
            {
               vos_mem_free((v_VOID_t*)limMsg->bodyptr);
               limMsg->bodyptr = NULL;
            }
            break;

       case SIR_LIM_ADDR2_MISS_IND:
       {
           limLog(pMac, LOGE,
                   FL("Addr2 mismatch interrupt received %X"),
                   limMsg->type);
           /*a message from HAL indicating addr2 mismatch interrupt occurred
             limMsg->bodyptr contains only pointer to 48-bit addr2 field*/

           vos_mem_free((v_VOID_t *)(limMsg->bodyptr));
           limMsg->bodyptr = NULL;
           break;
       }

#ifdef WLAN_FEATURE_VOWIFI_11R
    case WDA_AGGR_QOS_RSP:
       limProcessFTAggrQoSRsp( pMac, limMsg );
       break;
#endif

    case WDA_SET_LINK_STATE_RSP:
       linkStateParams = (tLinkStateParams *)limMsg->bodyptr;
#if defined WLAN_FEATURE_VOWIFI_11R
       pSession = linkStateParams->session;
       if(linkStateParams->ft
#if defined WLAN_FEATURE_ROAM_OFFLOAD
          && !pSession->bRoamSynchInProgress
#endif
         )
       {
          limSendReassocReqWithFTIEsMgmtFrame(pMac,
                                              pSession->pLimMlmReassocReq,
                                              pSession);
       }
#endif
       if( linkStateParams->callback )
       {
          linkStateParams->callback(pMac, linkStateParams->callbackArg,
                                    linkStateParams->status);
       }
       vos_mem_free((v_VOID_t *)(limMsg->bodyptr));
       limMsg->bodyptr = NULL;
       break;

#ifdef WLAN_FEATURE_PACKET_FILTERING
    case WDA_PACKET_COALESCING_FILTER_MATCH_COUNT_RSP:
        pmmProcessMessage(pMac, limMsg);
        break;
#endif // WLAN_FEATURE_PACKET_FILTERING

#ifdef WLAN_FEATURE_GTK_OFFLOAD
    case WDA_GTK_OFFLOAD_GETINFO_RSP:
        pmmProcessMessage(pMac, limMsg);
        break;
#endif // WLAN_FEATURE_GTK_OFFLOAD
    case eWNI_SME_SET_BCN_FILTER_REQ:
        {
#ifdef WLAN_ACTIVEMODE_OFFLOAD_FEATURE
            tpPESession     psessionEntry;
            tANI_U8 sessionId = (tANI_U8)limMsg->bodyval ;
            psessionEntry = &pMac->lim.gpSession[sessionId];
            if(psessionEntry != NULL && IS_ACTIVEMODE_OFFLOAD_FEATURE_ENABLE)
            {
               // sending beacon filtering information down to HAL
               if (limSendBeaconFilterInfo(pMac, psessionEntry) != eSIR_SUCCESS)
               {
                  limLog(pMac, LOGE, FL("Fail to send Beacon Filter Info "));
               }
            }
            vos_mem_free((v_VOID_t *)(limMsg->bodyptr));
            limMsg->bodyptr = NULL;
#endif
        }
        break;
#ifdef FEATURE_WLAN_TDLS
        case WDA_SET_TDLS_LINK_ESTABLISH_REQ_RSP:
        {
            tpPESession     psessionEntry;
            tANI_U8         sessionId;
            tTdlsLinkEstablishParams *pTdlsLinkEstablishParams;
            pTdlsLinkEstablishParams = (tTdlsLinkEstablishParams*) limMsg->bodyptr;

            if((psessionEntry = peFindSessionByStaId(pMac,
                                                     pTdlsLinkEstablishParams->staIdx,
                                                     &sessionId))== NULL)
            {
                limLog(pMac, LOGE, FL("session %u does not exist"), sessionId);
                /* Still send the eWNI_SME_TDLS_LINK_ESTABLISH_RSP message to
                 * SME with session id as zero and status as FAILURE so,
                 * that message queued in SME queue can be freed to prevent
                 * the SME cmd buffer leak */
                limSendSmeTdlsLinkEstablishReqRsp(pMac,
                                                  0,
                                                  NULL,
                                                  NULL,
                                                  eSIR_FAILURE);
            }
            else
            {
                limSendSmeTdlsLinkEstablishReqRsp(pMac,
                                                  psessionEntry->smeSessionId,
                                                  NULL,
                                                  NULL,
                                                  pTdlsLinkEstablishParams->status) ;
            }
            vos_mem_free((v_VOID_t *)(limMsg->bodyptr));
            limMsg->bodyptr = NULL;
            break;
        }
#endif

    case WDA_RX_SCAN_EVENT:
        limProcessRxScanEvent(pMac, limMsg->bodyptr);
        break;

    case WDA_IBSS_PEER_INACTIVITY_IND:
    {
       limProcessIbssPeerInactivity(pMac, limMsg->bodyptr);
       vos_mem_free((v_VOID_t *)(limMsg->bodyptr));
       limMsg->bodyptr = NULL;
       break;
    }

   case WDA_DFS_RADAR_IND:
        limSendSmeDfsEventNotify(pMac, limMsg->type,
                          (void *)limMsg->bodyptr);
        /* limmsg->bodyptr will be freed up by SME/CSR */
        break;

    case WDA_DFS_BEACON_TX_SUCCESS_IND:
        limProcessBeaconTxSuccessInd(pMac, limMsg->type,
                              (void *)limMsg->bodyptr);
        vos_mem_free((v_VOID_t*)limMsg->bodyptr);
        limMsg->bodyptr = NULL;
        break;

    case WDA_DISASSOC_TX_COMP:
        limDisassocTxCompleteCnf(pMac , limMsg->bodyval);
        break;

    case WDA_DEAUTH_TX_COMP:
        limDeauthTxCompleteCnf(pMac, limMsg->bodyval);
        break;

    case eWNI_SME_DFS_BEACON_CHAN_SW_IE_REQ:
        limProcessSmeReqMessages(pMac, limMsg);
        vos_mem_free((v_VOID_t*)limMsg->bodyptr);
        limMsg->bodyptr = NULL;
        break;

    case eWNI_SME_CHANNEL_CHANGE_REQ:
        limProcessSmeReqMessages(pMac, limMsg);
        vos_mem_free((v_VOID_t*)limMsg->bodyptr);
        limMsg->bodyptr = NULL;
        break;

    case eWNI_SME_START_BEACON_REQ:
        limProcessSmeReqMessages(pMac, limMsg);
        vos_mem_free((v_VOID_t*)limMsg->bodyptr);
        limMsg->bodyptr = NULL;
        break;

    case eWNI_SME_UPDATE_ADDITIONAL_IES:
        limProcessSmeReqMessages(pMac, limMsg);
        vos_mem_free((v_VOID_t*)limMsg->bodyptr);
        limMsg->bodyptr = NULL;
        break;

    case eWNI_SME_MODIFY_ADDITIONAL_IES:
        limProcessSmeReqMessages(pMac, limMsg);
        vos_mem_free((v_VOID_t*)limMsg->bodyptr);
        limMsg->bodyptr = NULL;
        break;

#ifdef QCA_HT_2040_COEX
    case eWNI_SME_SET_HT_2040_MODE:
        limProcessSmeReqMessages(pMac, limMsg);
        vos_mem_free((v_VOID_t*)limMsg->bodyptr);
        limMsg->bodyptr = NULL;
        break;
#endif

#ifdef SAP_AUTH_OFFLOAD
    case WDA_SAP_OFL_ADD_STA:
        lim_sap_offload_add_sta(pMac, limMsg);
        break;
    case WDA_SAP_OFL_DEL_STA:
        lim_sap_offload_del_sta(pMac, limMsg);
        break;
#endif /* SAP_AUTH_OFFLOAD */
    default:
        vos_mem_free((v_VOID_t*)limMsg->bodyptr);
        limMsg->bodyptr = NULL;
        // Unwanted messages
        // Log error
        limLog(pMac, LOGE,
                FL("Discarding unexpected message received %X"),
                limMsg->type);
        break;

    } // switch (limMsg->type)

   PELOG2(limLog(pMac, LOG2, FL("Done Processing msgType = %d, mlm state = %s"),
            limMsg->type,
            limMlmStateStr(pMac->lim.gLimMlmState));)

} /*** end limProcessMessages() ***/



/**
 * limProcessDeferredMessageQueue
 *
 *FUNCTION:
 * This function is called by LIM while exiting from Learn
 * mode. This function fetches messages posted to the LIM
 * deferred message queue limDeferredMsgQ.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pMac - Pointer to Global MAC structure
 * @return None
 */

void
limProcessDeferredMessageQueue(tpAniSirGlobal pMac)
{
    tSirMsgQ  limMsg = { 0, 0, 0 };

    tSirMsgQ *readMsg;
    tANI_U16  size;

    /*
    ** check any deferred messages need to be processed
    **/
    size = pMac->lim.gLimDeferredMsgQ.size;
    if (size > 0)
    {
        while ((readMsg = limReadDeferredMsgQ(pMac)) != NULL)
        {
            vos_mem_copy((tANI_U8*) &limMsg,
                    (tANI_U8*) readMsg, sizeof(tSirMsgQ));
            size--;
            limProcessMessages(pMac, &limMsg);

            if((limIsSystemInScanState(pMac)) || (true != GET_LIM_PROCESS_DEFD_MESGS(pMac)) ||
                  (pMac->lim.gLimSystemInScanLearnMode) ||
                   pMac->lim.gLimAddtsSent)
                break;
        }
    }
} /*** end limProcessDeferredMessageQueue() ***/


/*
 * limProcessNormalHddMsg
 * Function: this function checks the current lim state and decide whether the message passed shall be deffered.
 * @param  pMac - Pointer to Global MAC structure
 *         pLimMsg -- the message need to be processed
 *         fRspReqd -- whether return result to hdd
 * @return None
 */
void limProcessNormalHddMsg(tpAniSirGlobal pMac, tSirMsgQ *pLimMsg, tANI_U8 fRspReqd)
{
    tANI_BOOLEAN fDeferMsg = eANI_BOOLEAN_TRUE;

    /* Added For BT-AMP Support */
    if ((pMac->lim.gLimSystemRole == eLIM_AP_ROLE) ||(pMac->lim.gLimSystemRole == eLIM_BT_AMP_AP_ROLE )
                                                   ||(pMac->lim.gLimSystemRole == eLIM_BT_AMP_STA_ROLE)
                                                   ||(pMac->lim.gLimSystemRole == eLIM_UNKNOWN_ROLE))
    {
        /** This check is required only for the AP and in 2 cases.
         * 1. If we are in learn mode and we receive any of these messages,
         * you have to come out of scan and process the message, hence dont
         * defer the message here. In handler, these message could be defered
         * till we actually come out of scan mode.
         * 2. If radar is detected, you might have to defer all of these
         * messages except Stop BSS request/ Switch channel request. This
         * decision is also made inside its handler.
         *
         * Please be careful while using the flag fDeferMsg. Possibly you
         * might end up in an infinite loop.
         **/
        if (((pLimMsg->type == eWNI_SME_START_BSS_REQ) ||
             (pLimMsg->type == eWNI_SME_STOP_BSS_REQ) ||
             (pLimMsg->type == eWNI_SME_SWITCH_CHL_REQ) ||
             (pLimMsg->type == eWNI_SME_SWITCH_CHL_CB_SECONDARY_REQ) ||
             (pLimMsg->type == eWNI_SME_SWITCH_CHL_CB_PRIMARY_REQ)))
        {
            fDeferMsg = eANI_BOOLEAN_FALSE;
        }
    }

    /* limInsystemInscanState() refers the psessionEntry,  how to get session Entry????*/
    if (((pMac->lim.gLimAddtsSent) || (limIsSystemInScanState(pMac))) &&
        fDeferMsg) {
        // System is in DFS (Learn) mode or awaiting addts response
        // or if radar is detected, Defer processsing this message
        if (limDeferMsg(pMac, pLimMsg) != TX_SUCCESS)
        {
#ifdef WLAN_DEBUG
            pMac->lim.numSme++;
#endif
            PELOGE(limLog(pMac, LOGE, FL("Unable to Defer message(0x%X) limSmeState %d (prev sme state %d) sysRole %d mlm state %d (prev mlm state %d)"),
                        pLimMsg->type, pMac->lim.gLimSmeState,  pMac->lim.gLimPrevSmeState,
                        pMac->lim.gLimSystemRole,  pMac->lim.gLimMlmState,  pMac->lim.gLimPrevMlmState);)
            limLogSessionStates(pMac);
            // Release body
            vos_mem_free(pLimMsg->bodyptr);
            pLimMsg->bodyptr = NULL;
        }
    }
    else
    {
        if(fRspReqd)
        {
            // These messages are from HDD
            // Since these requests may also be generated
            // internally within LIM module, need to
            // distinquish and send response to host
            pMac->lim.gLimRspReqd = eANI_BOOLEAN_TRUE;
        }
#ifdef WLAN_DEBUG
        pMac->lim.numSme++;
#endif
        if(limProcessSmeReqMessages(pMac, pLimMsg))
        {
            // Release body
            // limProcessSmeReqMessage consumed the buffer. We can free it.
            vos_mem_free(pLimMsg->bodyptr);
            pLimMsg->bodyptr = NULL;
        }
    }
}

void
handleHTCapabilityandHTInfo(struct sAniSirGlobal *pMac, tpPESession psessionEntry)
{
    tSirMacHTCapabilityInfo macHTCapabilityInfo;
    tSirMacHTParametersInfo macHTParametersInfo;
    tSirMacHTInfoField1 macHTInfoField1;
    tSirMacHTInfoField2 macHTInfoField2;
    tSirMacHTInfoField3 macHTInfoField3;
    tANI_U32  cfgValue;
    tANI_U8 *ptr;

    if (wlan_cfgGetInt(pMac, WNI_CFG_HT_CAP_INFO, &cfgValue) != eSIR_SUCCESS)
    {
        limLog(pMac, LOGP, FL("Fail to retrieve WNI_CFG_HT_CAP_INFO value"));
        return ;
    }
    ptr = (tANI_U8 *) &macHTCapabilityInfo;
    *((tANI_U16 *)ptr) =  (tANI_U16) (cfgValue & 0xffff);
    pMac->lim.gHTLsigTXOPProtection = (tANI_U8)macHTCapabilityInfo.lsigTXOPProtection;
    pMac->lim.gHTMIMOPSState = (tSirMacHTMIMOPowerSaveState) macHTCapabilityInfo.mimoPowerSave;
    pMac->lim.gHTGreenfield = (tANI_U8)macHTCapabilityInfo.greenField;
    pMac->lim.gHTMaxAmsduLength = (tANI_U8)macHTCapabilityInfo.maximalAMSDUsize;
    pMac->lim.gHTShortGI20Mhz = (tANI_U8)macHTCapabilityInfo.shortGI20MHz;
    pMac->lim.gHTShortGI40Mhz = (tANI_U8)macHTCapabilityInfo.shortGI40MHz;
    pMac->lim.gHTPSMPSupport = (tANI_U8)macHTCapabilityInfo.psmp;
    pMac->lim.gHTDsssCckRate40MHzSupport = (tANI_U8)macHTCapabilityInfo.dsssCckMode40MHz;

    if (wlan_cfgGetInt(pMac, WNI_CFG_HT_AMPDU_PARAMS, &cfgValue) != eSIR_SUCCESS)
    {
        limLog(pMac, LOGP, FL("Fail to retrieve WNI_CFG_HT_PARAM_INFO value"));
        return ;
    }
    ptr = (tANI_U8 *) &macHTParametersInfo;
    *ptr =  (tANI_U8) (cfgValue & 0xff);
    pMac->lim.gHTAMpduDensity = (tANI_U8)macHTParametersInfo.mpduDensity;
    pMac->lim.gHTMaxRxAMpduFactor = (tANI_U8)macHTParametersInfo.maxRxAMPDUFactor;

    // Get HT IE Info
    if (wlan_cfgGetInt(pMac, WNI_CFG_HT_INFO_FIELD1, &cfgValue) != eSIR_SUCCESS)
    {
        limLog(pMac, LOGP, FL("Fail to retrieve WNI_CFG_HT_INFO_FIELD1 value"));
        return ;
    }
    ptr = (tANI_U8 *) &macHTInfoField1;
    *((tANI_U8 *)ptr) =  (tANI_U8) (cfgValue & 0xff);
    pMac->lim.gHTServiceIntervalGranularity = (tANI_U8)macHTInfoField1.serviceIntervalGranularity;
    pMac->lim.gHTControlledAccessOnly = (tANI_U8)macHTInfoField1.controlledAccessOnly;
    pMac->lim.gHTRifsMode = (tANI_U8)macHTInfoField1.rifsMode;

    if (wlan_cfgGetInt(pMac, WNI_CFG_HT_INFO_FIELD2, &cfgValue) != eSIR_SUCCESS)
    {
        limLog(pMac, LOGP, FL("Fail to retrieve WNI_CFG_HT_INFO_FIELD2 value"));
        return ;
    }
    ptr = (tANI_U8 *) &macHTInfoField2;
    *((tANI_U16 *)ptr) = (tANI_U16) (cfgValue & 0xffff);
    pMac->lim.gHTOperMode = (tSirMacHTOperatingMode) macHTInfoField2.opMode;

    if (wlan_cfgGetInt(pMac, WNI_CFG_HT_INFO_FIELD3, &cfgValue) != eSIR_SUCCESS)
    {
        limLog(pMac, LOGP, FL("Fail to retrieve WNI_CFG_HT_INFO_FIELD3 value"));
        return ;
    }
    ptr = (tANI_U8 *) &macHTInfoField3;
    *((tANI_U16 *)ptr) = (tANI_U16) (cfgValue & 0xffff);
    pMac->lim.gHTPCOActive = (tANI_U8)macHTInfoField3.pcoActive;
    pMac->lim.gHTPCOPhase = (tANI_U8)macHTInfoField3.pcoPhase;
    pMac->lim.gHTSecondaryBeacon = (tANI_U8)macHTInfoField3.secondaryBeacon;
    pMac->lim.gHTDualCTSProtection = (tANI_U8)macHTInfoField3.dualCTSProtection;
    pMac->lim.gHTSTBCBasicMCS = (tANI_U8)macHTInfoField3.basicSTBCMCS;

    /* The lim globals for channelwidth and secondary chnl have been removed and should not be used during no session;
     * instead direct cfg is read and used when no session for transmission of mgmt frames (same as old);
     * For now, we might come here during init and join with sessionEntry = NULL; in that case just fill the globals which exist
     * Sessionized entries values will be filled in join or add bss req. The ones which are missed in join are filled below
     */
    if (psessionEntry != NULL)
    {
        psessionEntry->htCapability = IS_DOT11_MODE_HT(psessionEntry->dot11mode);
        psessionEntry->beaconParams.fLsigTXOPProtectionFullSupport = (tANI_U8)macHTInfoField3.lsigTXOPProtectionFullSupport;
    }
}

void limLogSessionStates(tpAniSirGlobal pMac)
{
#ifdef WLAN_DEBUG
    int i;

    for(i = 0; i < pMac->lim.maxBssId; i++)
    {
        if(pMac->lim.gpSession[i].valid)
        {
            PELOG1(limLog(pMac, LOG1, FL("Session[%d] sysRole(%d) limSmeState %d (prev sme state %d) mlm state %d (prev mlm state %d)"),
                   i, pMac->lim.gpSession[i].limSystemRole,  pMac->lim.gpSession[i].limSmeState,
                   pMac->lim.gpSession[i].limPrevSmeState,   pMac->lim.gpSession[i].limMlmState,
                   pMac->lim.gpSession[i].limPrevMlmState);)
        }
    }
#endif //ifdef WLAN_DEBUG
}
