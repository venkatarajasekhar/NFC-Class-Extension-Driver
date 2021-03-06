/*
*          Modifications Copyright (c) Microsoft. All rights reserved.
*
*              Original code Copyright (c), NXP Semiconductors
*/

#include "phLibNfc_Pch.h"

#include "phHciNfc_Pipe.tmh"

static void phHciNfc_AnySetParameterCb(void *pContext,NFCSTATUS wStatus, void *pInfo);
static void phHciNfc_OpenPipeCb(void *pContext,NFCSTATUS wStatus, void *pInfo);

static NFCSTATUS
phHciNfc_CheckPropGate(uint8_t bGateId,
                       pphHciNfc_HciContext_t pHciCtxt);

static NFCSTATUS
phHciNfc_GetPipeIndex(phHciNfc_RegInfo_t *pList,
                        uint8_t bNumOfEntries,
                        uint8_t bPipeId,
                        uint8_t *pIndex);

static NFCSTATUS
phHciNfc_ProcessPipeCreateNotifyCmd(phHciNfc_ReceiveParams_t *pReceivedParams,
                                    pphHciNfc_HciContext_t    pHciContext);

static NFCSTATUS
phHciNfc_ProcessClearAllPipeNotifyCmd(phHciNfc_ReceiveParams_t *pReceivedParams,
                                      pphHciNfc_HciContext_t    pHciContext);

static void phHciNfc_AnyOkCb(void *pContext, NFCSTATUS wStatus)
{
    UNUSED(pContext);
    UNUSED(wStatus);
}

void phHciNfc_CmdSendCb(void *pContext, NFCSTATUS wStatus)
{
    pphHciNfc_HciContext_t pHciContext=NULL;
    pphHciNfc_RspCb_t pClientCb;
    phHciNfc_HciRegData_t tHciRegData;

    pHciContext = (pphHciNfc_HciContext_t)pContext;
    pClientCb = pHciContext->Cb_Info.pClientInitCb;
    PH_LOG_HCI_FUNC_ENTRY();

    if((NULL != pHciContext) && (NULL != pHciContext ->pSendParams) && (NULL != pClientCb))
    {
        if( NFCSTATUS_SUCCESS == wStatus)
        {
            if(NULL != pHciContext->SendCb_Info.pClientInitCb)
            {
                tHciRegData.eMsgType = (phNciNfc_eHciMsgType_t)phHciNfc_e_HciMsgTypeRsp;
                tHciRegData.bPipeId = pHciContext->pSendParams->bPipeId;
                wStatus = phHciNfc_RegisterCmdRspEvt((void*)pHciContext,
                                        &tHciRegData,
                                        pHciContext->SendCb_Info.pClientInitCb,
                                        (void*)pHciContext);
                if(NFCSTATUS_SUCCESS != wStatus)
                {
                    pClientCb((void*)pHciContext->Cb_Info.pClientCntx, NFCSTATUS_FAILED, NULL);
                }
            }
            else
            {
                pClientCb(pHciContext->Cb_Info.pClientCntx, wStatus, NULL);
            }
        }
        else
        {
            pClientCb(pHciContext->Cb_Info.pClientCntx, NFCSTATUS_FAILED, NULL);
        }
    }
    PH_LOG_HCI_FUNC_EXIT();
}

static
void phHciNfc_AnySetParameterCb(void *pContext,NFCSTATUS wStatus, void *pInfo)
{
    NFCSTATUS wIntStatus = NFCSTATUS_FAILED;
    pphHciNfc_HciContext_t pHciContext = NULL;
    phHciNfc_ReceiveParams_t *pReceivedParams = (phHciNfc_ReceiveParams_t *) pInfo;
    pphHciNfc_RspCb_t pClientCb = NULL;
    PH_LOG_HCI_FUNC_ENTRY();

    if(NULL != pContext)
    {
        pHciContext = (pphHciNfc_HciContext_t)pContext;
        pClientCb = pHciContext->Cb_Info.pClientInitCb;
        phOsalNfc_FreeMemory(pHciContext->pSendParams);

        if(NFCSTATUS_SUCCESS == wStatus)
        {
            /* Check for Response ANY_OK */
            if(phHciNfc_e_RspAnyOk == (phHciNfc_RspType_t)(pReceivedParams->bIns))
            {
                wIntStatus = NFCSTATUS_SUCCESS;
            }
        }
        else
        {
            wIntStatus = NFCSTATUS_FAILED;
        }

        if(NULL != pClientCb)
        {
            /* Invoke upper layer callback function */
            pHciContext->Cb_Info.pClientInitCb = NULL;
            pClientCb(pHciContext->Cb_Info.pClientCntx, wIntStatus, NULL);
        }
    }
    PH_LOG_HCI_FUNC_EXIT();
}

static
void phHciNfc_AnyGetParameterCb(void *pContext,NFCSTATUS wStatus, void *pInfo)
{
    NFCSTATUS wIntStatus = NFCSTATUS_FAILED;
    pphHciNfc_HciContext_t pHciContext = NULL;
    phHciNfc_ReceiveParams_t *pReceivedParams = (phHciNfc_ReceiveParams_t *) pInfo;
    pphHciNfc_RspCb_t pClientCb = NULL;
    PH_LOG_HCI_FUNC_ENTRY();

    if(NULL != pContext)
    {
        pHciContext = (pphHciNfc_HciContext_t)pContext;
        pClientCb = pHciContext->Cb_Info.pClientInitCb;
        phOsalNfc_FreeMemory(pHciContext->pSendParams);

        if(NFCSTATUS_SUCCESS == wStatus)
        {
            if(phHciNfc_e_RspAnyOk == (phHciNfc_RspType_t)(pReceivedParams->bIns))
            {
                wIntStatus = NFCSTATUS_SUCCESS;
            }
        }
        else
        {
            wIntStatus = NFCSTATUS_FAILED;
        }

        if(NULL != pClientCb)
        {
            /* Invoke upper layer callback function */
            pHciContext->Cb_Info.pClientInitCb = NULL;
            pClientCb(pHciContext->Cb_Info.pClientCntx, wIntStatus, (void *)pReceivedParams);
        }
    }
    PH_LOG_HCI_FUNC_EXIT();
}

static
void phHciNfc_OpenPipeCb(void *pContext,NFCSTATUS wStatus, void *pInfo)
{
    NFCSTATUS wIntStatus = NFCSTATUS_FAILED;
    pphHciNfc_HciContext_t pHciContext = NULL;
    phHciNfc_ReceiveParams_t *pReceivedParams = (phHciNfc_ReceiveParams_t *) pInfo;
    pphHciNfc_RspCb_t pClientCb = NULL;
    uint8_t noOfOpenPipes=0;
    PH_LOG_HCI_FUNC_ENTRY();

    if(NULL != pContext)
    {
        pHciContext = (pphHciNfc_HciContext_t)pContext;
        pClientCb = pHciContext->Cb_Info.pClientInitCb;
        phOsalNfc_FreeMemory(pHciContext->pSendParams);

        /*Check for Receive Response*/
        if(NFCSTATUS_SUCCESS == wStatus)
        {
            if(phHciNfc_e_RspAnyOk == (phHciNfc_RspType_t)(pReceivedParams->bIns))
            {
                /* '0' length would be received if destination type is a host controller */
                if(0 != pReceivedParams->wLen)
                {
                    noOfOpenPipes = pReceivedParams->pData[0];
                }
                wIntStatus = NFCSTATUS_SUCCESS;
            }
        }
        else
        {
            wIntStatus = NFCSTATUS_FAILED;
        }

        if(NULL != pClientCb)
        {
            /* Invoke upper layer callback function */
            pHciContext->Cb_Info.pClientInitCb = NULL;
            pClientCb(pHciContext->Cb_Info.pClientCntx, wIntStatus, &noOfOpenPipes);
        }
    }
    PH_LOG_HCI_FUNC_EXIT();
}

NFCSTATUS
phHciNfc_Transceive(void    *pHciContext,
                    uint8_t    bpipeId,
                    uint8_t    bEvent,
                    uint32_t   dwDataLen,
                    uint8_t     *pData,
                    pphHciNfc_RspCb_t   pRspCb,
                    void    *pContext
                    )
{
    NFCSTATUS    wStatus = NFCSTATUS_SUCCESS;
    phHciNfc_SendParams_t tSendParams;
    pphHciNfc_HciContext_t pHciCtxt = (pphHciNfc_HciContext_t)pHciContext;
    UNUSED(pRspCb);
    UNUSED(pContext);
    PH_LOG_HCI_FUNC_ENTRY();

    if(NULL != pHciCtxt)
    {
        /* Data will flow on requesed pipe Id */
        tSendParams.bPipeId = bpipeId;
        /* bEvent is given as input can be properiatary */
        tSendParams.bIns = bEvent;
        /* Message Type is Event */
        tSendParams.bMsgType = phHciNfc_e_HcpMsgTypeEvent;
        /* Pointer to the data to be transfer*/
        tSendParams.pData = pData;
        /* Length data to be transfer*/
        tSendParams.dwLen = dwDataLen;

        /* Send the command*/
        wStatus = phHciNfc_CoreSend (pHciCtxt,&tSendParams,&phHciNfc_CmdSendCb, pHciCtxt);
        if(NFCSTATUS_PENDING == wStatus)
        {
            pHciCtxt->Cb_Info.pClientInitCb = NULL;
            pHciCtxt->Cb_Info.pClientCntx = NULL;
            pHciCtxt->SendCb_Info.pClientInitCb = NULL;
            pHciCtxt->SendCb_Info.pClientCntx = NULL;
        }
        else
        {
            /* If the status is not pending */
            return wStatus;
        }
    }
    else
    {
        wStatus = PHNFCSTVAL(CID_NFC_HCI, NFCSTATUS_INVALID_PARAMETER);
    }

    PH_LOG_HCI_FUNC_EXIT();
    return wStatus;
}
NFCSTATUS
phHciNfc_AnyGetParameter(
                        void  *pHciContext,
                        uint8_t bGateId,
                        uint8_t bIdentifier,
                        uint8_t bPipeId,
                        pphHciNfc_RspCb_t     pRspCb,
                        void            *pContext
                     )
{
    NFCSTATUS wStatus = NFCSTATUS_FAILED;
    phHciNfc_SendParams_t *pSendParams;
    pphHciNfc_HciContext_t pHciCtxt = (pphHciNfc_HciContext_t)pHciContext;
    PH_LOG_HCI_FUNC_ENTRY();

    if (NULL == pHciContext)
    {
        return wStatus;
    }
    
    switch(bGateId)
    {
        case PHHCINFC_LINK_MGMT_GATEID:
            if(bIdentifier!= 1)
            {
                return wStatus;
            }
        break;
        case PHHCINFC_ADM_GATEID:
            if((1 < bIdentifier)&&(4 < bIdentifier))
            {
                return wStatus;
            }
        break;
        case PHHCINFC_IDENTITY_MGMT_GATEID:
            if((1 < bIdentifier)&&(6 < bIdentifier))
            {
                return wStatus;
            }
        break;
        default:
            wStatus = phHciNfc_CheckPropGate(bGateId,pHciCtxt);
            if(wStatus != NFCSTATUS_SUCCESS)
            {
                return wStatus;
            }else
            {
                /* Continue to Process the received Get Param Command */
            }
        break;
    }

    pSendParams = (phHciNfc_SendParams_t *)phOsalNfc_GetMemory( sizeof(phHciNfc_SendParams_t) );
    if((NULL != pSendParams) )
    {
        /* Copy send parameters to hci context */
        pHciCtxt->pSendParams = pSendParams;
        /*Message Type is Command */
        pSendParams->bMsgType = phHciNfc_e_HcpMsgTypeCommand;
        /*Pipe id for the command */
        pSendParams->bPipeId = bPipeId;
        /*Command on the pipe */
        pSendParams->bIns = PHHCINFC_ANY_GET_PARAMETER;
        /* Process the Command */

        /*Form command for generic commands */
        (pSendParams)->pData= &bIdentifier;
        (pSendParams)->pData[0] = bIdentifier;
        (pSendParams) ->dwLen = 1;

        wStatus = phHciNfc_CoreSend (pHciCtxt,pSendParams,&phHciNfc_CmdSendCb, pHciCtxt);
        if(NFCSTATUS_PENDING == wStatus)
        {
            /* Store the call back and the context of upper layer that can be called later*/
            pHciCtxt->Cb_Info.pClientInitCb = pRspCb;
            pHciCtxt->Cb_Info.pClientCntx = pContext;
           /* Store the call back of the Send command can be called on send complete*/
            pHciCtxt->SendCb_Info.pClientInitCb =&phHciNfc_AnyGetParameterCb;
            pHciCtxt->SendCb_Info.pClientCntx = pHciCtxt;
        }
        else
        {
            /* If the status is not pending free the allocted memory */
            phOsalNfc_FreeMemory(pSendParams);
            return wStatus;
        }
    }
    PH_LOG_HCI_FUNC_EXIT();
    return wStatus;
}

static NFCSTATUS phHciNfc_CheckPropGate(uint8_t bGateId, pphHciNfc_HciContext_t pHciCtxt)
{
    NFCSTATUS wStatus = NFCSTATUS_FAILED;
    uint16_t  wCount;
    for(wCount = 0; wCount < PHHCINFC_MAX_PIPES; wCount++)
    {
        if(bGateId == pHciCtxt->aSEPipeList[wCount].bGateId)
        {
            wStatus = NFCSTATUS_SUCCESS;
            break;
        }
    }
    return wStatus;
}
NFCSTATUS
phHciNfc_AnySetParameter(
                        void  *pHciContext,
                        uint8_t bIdentifier,
                        uint8_t bPipeId,
                        uint8_t bSetDataLen,
                        uint8_t *pSetData,
                        pphHciNfc_RspCb_t pRspCb,
                        void *pContext
                     )
{
    NFCSTATUS wStatus = NFCSTATUS_FAILED;
    phHciNfc_SendParams_t *pSendParams;
    uint8_t aData[PHHCI_HCP_MAX_PACKET_SIZE];
    pphHciNfc_HciContext_t pHciCtxt = (pphHciNfc_HciContext_t)pHciContext;
    PH_LOG_HCI_FUNC_ENTRY();

    if (NULL == pHciContext)
    {
        return wStatus;
    }
    else if ((bSetDataLen + PHHCINFC_IDENTITY_LENGTH) > sizeof(aData))
    {
        wStatus = NFCSTATUS_INVALID_PARAMETER;
        return wStatus;
    }
    else
    {
        pSendParams = (phHciNfc_SendParams_t *)phOsalNfc_GetMemory( sizeof(phHciNfc_SendParams_t) );
        if((NULL != pSendParams) )
        {
            /* Copy send parameters to hci context */
            pHciCtxt->pSendParams = pSendParams;
            /*Message Type is Command */
            pSendParams ->bMsgType = phHciNfc_e_HcpMsgTypeCommand;
            /*Pipe id for the command */
            pSendParams->bPipeId = bPipeId;
            /*Command on the pipe */
            pSendParams->bIns = PHHCINFC_ANY_SET_PARAMETER;

            /*Form command for generic commands */
            (pSendParams)->pData= aData;
            (pSendParams)->pData[0] = bIdentifier;

            if(PHHCINFC_PIPE_SESSION_INDEX == bIdentifier)
            {
                /* Check length for session identity is 8 */
                if(bSetDataLen == PHHCINFC_PIPE_SESSION_ID_LENGTH)
                {
                    (pSendParams)->dwLen = bSetDataLen + PHHCINFC_IDENTITY_LENGTH;
                }
                else
                {
                    wStatus = phHciNfc_e_RspAnyECmdParUnknown;
                    phOsalNfc_FreeMemory(pSendParams);
                    return wStatus;
                }
            }
            else if(PHHCINFC_PIPE_WHITELIST_INDEX == bIdentifier)
            {
                (pSendParams)->dwLen = bSetDataLen + PHHCINFC_IDENTITY_LENGTH;
            }
            else
            {
                wStatus = phHciNfc_e_RspAnyERegAccessDenied;
                phOsalNfc_FreeMemory(pSendParams);
                return wStatus;
            }
            /*Form command for generic commands */
            phOsalNfc_MemCopy( (uint8_t*)(aData+ PHHCINFC_PIPE_GENERICCMD_BYTE_OFFSET),(uint8_t*)
                                            (pSetData),bSetDataLen);

            wStatus = phHciNfc_CoreSend (pHciCtxt,pSendParams,&phHciNfc_CmdSendCb, pHciCtxt);
            if(NFCSTATUS_PENDING == wStatus)
            {
                /* Store the call back and the context of upper layer that can be called later*/
                pHciCtxt->Cb_Info.pClientInitCb = pRspCb;
                pHciCtxt->Cb_Info.pClientCntx = pContext;
               /* Store the call back of the Send command can be called on send complete*/
                pHciCtxt->SendCb_Info.pClientInitCb =&phHciNfc_AnySetParameterCb;
                pHciCtxt->SendCb_Info.pClientCntx = pHciCtxt;
            }
            else
            {
                /* If the status is not pending free the allocted memory */
                phOsalNfc_FreeMemory(pSendParams);
                return wStatus;
            }
        }
    }
    PH_LOG_HCI_FUNC_EXIT();
    return wStatus;
}


NFCSTATUS
phHciNfc_OpenPipe(
                        void  *pHciContext,
                        uint8_t  bPipeId,
                        pphHciNfc_RspCb_t     pRspCb,
                        void            *pContext
                     )
{
    NFCSTATUS wStatus = NFCSTATUS_FAILED;
    phHciNfc_SendParams_t *pSendParams;
    pphHciNfc_HciContext_t pHciCtxt = (pphHciNfc_HciContext_t)pHciContext;
    pSendParams = (phHciNfc_SendParams_t *)phOsalNfc_GetMemory( sizeof(phHciNfc_SendParams_t) );
    PH_LOG_HCI_FUNC_ENTRY();

    /* Copy send parameters to hci context */
    pHciCtxt->pSendParams = pSendParams;
    /*Message Type is Command */
    pSendParams->bMsgType = phHciNfc_e_HcpMsgTypeCommand;
    /*Pipe id for the command */
    pSendParams->bPipeId = bPipeId;
    /*Instruction on the pipe */
    pSendParams->bIns = PHHCINFC_ANY_OPEN_PIPE;

    if(NULL != pSendParams)
    {
        /*Command allows Host to open a closed pipe */
        (pSendParams)->pData = NULL;
        (pSendParams)->dwLen = 0;
        wStatus = phHciNfc_CoreSend (pHciCtxt,pSendParams,&phHciNfc_CmdSendCb, pHciCtxt);
        if(NFCSTATUS_PENDING == wStatus)
        {
            /* Store the call back and the context of upper layer that can be called later*/
            pHciCtxt->Cb_Info.pClientInitCb = pRspCb;
            pHciCtxt->Cb_Info.pClientCntx = pContext;
           /* Store the call back of the Send command can be called on send complete*/
            pHciCtxt->SendCb_Info.pClientInitCb =&phHciNfc_OpenPipeCb;
            pHciCtxt->SendCb_Info.pClientCntx = pHciCtxt;
        }
        else
        {
            phOsalNfc_FreeMemory(pSendParams);
            return wStatus;
        }
    }

    PH_LOG_HCI_FUNC_EXIT();
    return wStatus;
}
void phHciNfc_ReceiveHandler(void *pContext,phHciNfc_ReceiveParams_t *pReceivedParams, NFCSTATUS wStatus)
{
    uint8_t bIndex;
    uint8_t bNumOfEntries=0;
    phHciNfc_RegInfo_t *pList = NULL;
    pphHciNfc_HciContext_t pHciCtx = (pphHciNfc_HciContext_t)pContext;
    NFCSTATUS wIntStatus = NFCSTATUS_FAILED;
    PH_LOG_HCI_FUNC_ENTRY();

    if((NULL != pHciCtx) && (NULL != pReceivedParams))
    {
        /* Get the message type from the received message */
        switch(pReceivedParams->bMsgType)
        {
        case phHciNfc_e_HciMsgTypeCmd:
            /* Get Command list from Hci Context*/
            pList = pHciCtx->aHciCmdRegList;
            bNumOfEntries = PHHCINFC_MAX_CMD_REGS;
            wIntStatus = NFCSTATUS_SUCCESS;
            break;
        case phHciNfc_e_HciMsgTypeRsp:
            /* Get Response list from Hci Context*/
            pList = pHciCtx->aHciRspRegList;
            bNumOfEntries = PHHCINFC_MAX_RSP_REGS;
            wIntStatus = NFCSTATUS_SUCCESS;
            break;
        case phHciNfc_e_HciMsgTypeEvent:
            /* Get Event list from Hci Context*/
            pList = pHciCtx->aHciEvtRegList;
            bNumOfEntries = PHHCINFC_MAX_EVT_REGS;
            wIntStatus = NFCSTATUS_SUCCESS;
            break;
        default:
            /* Invalid message type */
            wIntStatus = NFCSTATUS_INVALID_PARAMETER;
            break;
        }

        if((NFCSTATUS_SUCCESS == wIntStatus) &&(NULL != pList))
        {
            /* Get the Pipe index for the function registered against the pipe*/
            wIntStatus = phHciNfc_GetPipeIndex(pList,bNumOfEntries,pReceivedParams->bPipeId, &bIndex);
            if((NFCSTATUS_SUCCESS == wIntStatus) && (NULL != pList[bIndex].pNotifyCb))
            {
                /* Execute the registered function for the pipe */
                pList[bIndex].pNotifyCb(pList[bIndex].pContext,wStatus,(void *)pReceivedParams);
                /* Unregistered the function for the pipe and clear the data */
                if(phHciNfc_e_HciMsgTypeRsp == pReceivedParams->bMsgType)
                {
                    pList[bIndex].bEnabled = 0;
                    pList[bIndex].bPipeId = 0;
                    pList[bIndex].pContext = NULL;
                    pList[bIndex].pNotifyCb = NULL;
                }
            }
        }
    }
    PH_LOG_HCI_FUNC_EXIT();
}

NFCSTATUS
phHciNfc_RegisterCmdRspEvt(
                        void                    *pHciContext,
                        pphHciNfc_HciRegData_t  pHciRegData,
                        pphHciNfc_RspCb_t       pRegisterCb,
                        void                    *pContext)
{
    NFCSTATUS wStatus = NFCSTATUS_SUCCESS;
    pphHciNfc_RegInfo_t pList = NULL;
    pphHciNfc_HciContext_t pHciCtx = (pphHciNfc_HciContext_t)pHciContext;
    uint8_t bNumOfEntries = 0;
    PH_LOG_HCI_FUNC_ENTRY();

    if((NULL != pHciCtx) && (NULL != pRegisterCb) && (NULL != pHciRegData))
    {
        switch(pHciRegData->eMsgType)
        {
        case phHciNfc_e_HciMsgTypeCmd:
            /* Get Command list from Hci Context*/
            pList = pHciCtx->aHciCmdRegList;
            bNumOfEntries = PHHCINFC_MAX_CMD_REGS;
            break;
        case phHciNfc_e_HciMsgTypeRsp:
            /* Get Response list from Hci Context*/
            pList = pHciCtx->aHciRspRegList;
            bNumOfEntries = PHHCINFC_MAX_RSP_REGS;
            break;
        case phHciNfc_e_HciMsgTypeEvent:
            /* Get Event list from Hci Context*/
            pList = pHciCtx->aHciEvtRegList;
            bNumOfEntries = PHHCINFC_MAX_EVT_REGS;
            break;
        default:
            /* Invalid message type */
            wStatus = NFCSTATUS_INVALID_PARAMETER;
            break;
        }
    }
    else
    {
        /* Invalid input parameter */
        wStatus = NFCSTATUS_INVALID_PARAMETER;
    }

    if((NFCSTATUS_SUCCESS == wStatus) && (NULL != pContext) && (NULL != pList)
       && (0 != bNumOfEntries) && (NULL != pHciRegData) && (NULL != pRegisterCb))
    {
        /* Register the function against the pipe Id */
        wStatus = phHciNfc_AddRegistration(pList,bNumOfEntries,pHciRegData->bPipeId,pRegisterCb,pContext);
    }
    PH_LOG_HCI_FUNC_EXIT();
    return wStatus;
}

NFCSTATUS
phHciNfc_VerifyIfRegistered(
                        phHciNfc_RegInfo_t *pList,
                        uint8_t             bNumOfEntries,
                        uint8_t bPipeId,
                        pphHciNfc_RspCb_t   pRspCb)
{
    NFCSTATUS wStatus = NFCSTATUS_FAILED;
    uint8_t bCount;
    PH_LOG_HCI_FUNC_ENTRY();

    if((NULL == pList) || (NULL == pRspCb))
    {
        /* Invalid input parameters */
        wStatus = NFCSTATUS_INVALID_PARAMETER;
    }
    else
    {
        for(bCount = 0; bCount < bNumOfEntries; bCount++)
        {
            if((1 == pList[bCount].bEnabled) && (bPipeId == pList[bCount].bPipeId))
            {
                if(pRspCb == pList[bCount].pNotifyCb)
                {
                    wStatus = NFCSTATUS_SUCCESS;
                    break;
                }
            }
        }
    }
    PH_LOG_HCI_FUNC_EXIT();
    return wStatus;
}

NFCSTATUS
phHciNfc_AddRegistration(
                        phHciNfc_RegInfo_t *pList,
                        uint8_t             bNumOfEntries,
                        uint8_t bPipeId,
                        pphHciNfc_RspCb_t   pRspCb,
                        void                *pContext)
{
    NFCSTATUS wStatus = NFCSTATUS_FAILED;
    uint8_t bCount;
    PH_LOG_HCI_FUNC_ENTRY();

    if((NULL == pList) || (NULL == pRspCb))
    {
        /* Invalid input parameters */
        wStatus = NFCSTATUS_INVALID_PARAMETER;
    }
    else
    {
        /* Check if any registration using the date details (pipe ID and call back function)
        already exists */
        wStatus = phHciNfc_VerifyIfRegistered(pList,bNumOfEntries,
                                              bPipeId,pRspCb);
        if(NFCSTATUS_SUCCESS != wStatus)
        {
            /* Find and empty slot and register call back function */
            for(bCount = 0; bCount < bNumOfEntries; bCount++)
            {
                /* Check for an empty slot */
                if(0 == pList[bCount].bEnabled)
                {
                    pList[bCount].bPipeId = bPipeId;
                    pList[bCount].pNotifyCb = pRspCb;
                    pList[bCount].pContext = pContext;
                    pList[bCount].bEnabled = 1;
                    wStatus = NFCSTATUS_SUCCESS;
                    break;
                }
            }
        }
    }
    PH_LOG_HCI_FUNC_EXIT();
    return wStatus;
}

NFCSTATUS
phHciNfc_UnRegisterCmdRspEvt(
                        void                    *pHciContext,
                        pphHciNfc_HciRegData_t  pHciRegData,
                        pphHciNfc_RspCb_t       pRegisterCb)
{
    NFCSTATUS wStatus = NFCSTATUS_SUCCESS;
    pphHciNfc_RegInfo_t pList = NULL;
    pphHciNfc_HciContext_t pHciCtx = (pphHciNfc_HciContext_t)pHciContext;
    uint8_t bNumOfEntries;

    PH_LOG_HCI_FUNC_ENTRY();

    if((NULL == pHciCtx) || (NULL == pRegisterCb) || (NULL == pHciRegData))
    {
        /* Invalid input parameter */
        wStatus = NFCSTATUS_INVALID_PARAMETER;
    }
    else
    {
        switch(pHciRegData->eMsgType)
        {
        case phHciNfc_e_HciMsgTypeCmd:
            /* Get Command list from Hci Context*/
            pList = pHciCtx->aHciCmdRegList;
            bNumOfEntries = PHHCINFC_MAX_CMD_REGS;
            break;
        case phHciNfc_e_HciMsgTypeRsp:
            /* Get Response list from Hci Context*/
            pList = pHciCtx->aHciRspRegList;
            bNumOfEntries = PHHCINFC_MAX_RSP_REGS;
            break;
        case phHciNfc_e_HciMsgTypeEvent:
            /* Get Event list from Hci Context*/
            pList = pHciCtx->aHciEvtRegList;
            bNumOfEntries = PHHCINFC_MAX_EVT_REGS;
            break;
        default:
            /* Invalid message type */
            wStatus = NFCSTATUS_INVALID_PARAMETER;
            bNumOfEntries = 0;
            break;
        }

        if((NFCSTATUS_SUCCESS == wStatus) && (NULL != pList))
        {
            /* Unregister the function against the pipe Id */
            wStatus = phHciNfc_RemoveRegistration(pList,bNumOfEntries,pHciRegData->bPipeId,pRegisterCb);
        }
    }

    PH_LOG_HCI_FUNC_EXIT();
    return wStatus;
}


NFCSTATUS
phHciNfc_RemoveRegistration(
                        phHciNfc_RegInfo_t *pList,
                        uint8_t             bNumOfEntries,
                        uint8_t bPipeId,
                        pphHciNfc_RspCb_t   pRspCb)
{
    NFCSTATUS wStatus = NFCSTATUS_FAILED;
    uint8_t bCount;
    PH_LOG_HCI_FUNC_ENTRY();

    if((NULL == pList) || (NULL == pRspCb))
    {
        /* Invalid input parameters */
        wStatus = NFCSTATUS_INVALID_PARAMETER;
    }
    else
    {
        for(bCount = 0; bCount < bNumOfEntries; bCount++)
        {
            /* Check for an Registered slot */
            if(1 == pList[bCount].bEnabled)
            {
                /* Check for Registered slot is on the same Pipe */
                if((pList[bCount].bPipeId == bPipeId) &&
                    (pList[bCount].pNotifyCb == pRspCb))
                {
                    /* Unregister the function against the pipe Id */
                    pList[bCount].pNotifyCb = NULL;
                    pList[bCount].pContext = NULL;
                    pList[bCount].bPipeId = 0;
                    /* Disable the flag */
                    pList[bCount].bEnabled = 0;
                    wStatus = NFCSTATUS_SUCCESS;
                    break;
                }
            }
        }
    }
    PH_LOG_HCI_FUNC_EXIT();
    return wStatus;
}

static NFCSTATUS phHciNfc_GetPipeIndex(phHciNfc_RegInfo_t *pList,
                        uint8_t             bNumOfEntries,
                        uint8_t bPipeId,
                        uint8_t *pIndex)
{
    NFCSTATUS wStatus = NFCSTATUS_INVALID_PARAMETER;
    uint8_t bCount=0;
    PH_LOG_HCI_FUNC_ENTRY();

    if((NULL != pList) && (NULL != pIndex))
    {
        for(bCount = 0; bCount < bNumOfEntries; bCount++)
        {
            /* Check if the slot is enabled */
            if((1 == pList[bCount].bEnabled) && (bPipeId == pList[bCount].bPipeId))
            {
                /* Copy the enabled slot number */
                (*pIndex) = bCount;
                wStatus = NFCSTATUS_SUCCESS;
                break;
            }
        }
        if(NFCSTATUS_SUCCESS != wStatus)
        {
            /* Enabled slot is not found */
            wStatus = NFCSTATUS_FAILED;
        }
    }
    PH_LOG_HCI_FUNC_EXIT();
    return wStatus;
}


 void
 phHciNfc_ReceiveAdminNotifyEvt( void *pContext,NFCSTATUS wStatus, void *pInfo)
{
    pphHciNfc_HciContext_t    pHciContext = (pphHciNfc_HciContext_t)pContext;
    phHciNfc_ReceiveParams_t *pReceivedParams = (phHciNfc_ReceiveParams_t *) pInfo;
    if( (NULL != pHciContext) && \
        (NULL != pInfo) )
    {
        if(NFCSTATUS_SUCCESS == wStatus)
        {
            /* Check for Event Hot Plug */
            if(phHciNfc_e_EvtHotPlug == (phHciNfc_EvtType_t)pReceivedParams->bIns)
            {
#if 0 /* Commented as waiting on EVENT_HOTPLUG is not required as of now */
                (void)phLibNfc_SeEventHotPlugCb(pHciContext,wStatus,pReceivedParams);
#endif
                wStatus = NFCSTATUS_SUCCESS;
            }
            else
            {
                wStatus = NFCSTATUS_FAILED;
            }
        }
    }
    else
    {
        wStatus = NFCSTATUS_FAILED;
    }
}

static NFCSTATUS
phHciNfc_ProcessPipeCreateNotifyCmd(phHciNfc_ReceiveParams_t *pReceivedParams,
                                    pphHciNfc_HciContext_t    pHciContext)
{
    phHciNfc_HciRegData_t tHciRegData;
    phHciNfc_AdmNotfPipeCrCmdParams_t tPipeCreatedNtfParams;
    uint8_t aSetHciSessionId[8];
    NFCSTATUS wIntStatus= NFCSTATUS_FAILED;
    NFCSTATUS wStatus = NFCSTATUS_FAILED;
    PH_LOG_HCI_FUNC_ENTRY();

    if((NULL != pReceivedParams->pData) && (PHHCINFC_CREATE_PIPE_REQ_LEN == pReceivedParams->wLen))
    {
        tPipeCreatedNtfParams.bDestGID    = ((phHciNfc_AdmNotfPipeCrCmdParams_t*)pReceivedParams->pData)->bDestGID;
        tPipeCreatedNtfParams.bDestHID    = ((phHciNfc_AdmNotfPipeCrCmdParams_t*)pReceivedParams->pData)->bDestHID;
        tPipeCreatedNtfParams.bPipeID     = ((phHciNfc_AdmNotfPipeCrCmdParams_t*)pReceivedParams->pData)->bPipeID;
        tPipeCreatedNtfParams.bSourceGID  = ((phHciNfc_AdmNotfPipeCrCmdParams_t*)pReceivedParams->pData)->bSourceGID;
        tPipeCreatedNtfParams.bSourceHID  = ((phHciNfc_AdmNotfPipeCrCmdParams_t*)pReceivedParams->pData)->bSourceHID;

        /* Register for Cmd Open pipe for the newly created pipe */
        tHciRegData.eMsgType = phHciNfc_e_HciMsgTypeCmd;
        tHciRegData.bPipeId = (uint8_t) pReceivedParams->pData[4];

        wStatus = phHciNfc_RegisterCmdRspEvt(pHciContext,
                                &tHciRegData,
                                &phHciNfc_ReceiveOpenPipeNotifyCmd,
                                pHciContext);

        if(NFCSTATUS_SUCCESS != wStatus)
        {
            /* Failed to regiter Adm pipe events */
            wIntStatus = phHciNfc_e_RspAnyERegAccessDenied;
        }

        phOsalNfc_MemCopy(aSetHciSessionId,pHciContext->aGetHciSessionId, PHHCINFC_PIPE_SESSIONID_LEN);

        /* Update Session ID Based on whether Pipe request received from UICC or eSE*/
        if( tPipeCreatedNtfParams.bSourceHID != phHciNfc_e_UICCHostID )
        {
            /* Pipe Created Ntf from eSE*/
            /* Check the Gate ID to which Pipe Request is received */
            if((tPipeCreatedNtfParams.bDestGID == phHciNfc_e_ApduGate))
            {
                /* Update the Session ID Pipe Presence Indicator Byte */
                aSetHciSessionId[PHHCI_PIPE_PRESENCE_INDEX] = SET_BITS8(aSetHciSessionId[PHHCI_PIPE_PRESENCE_INDEX],
                                                                      PHHCI_ESE_APDU_PIPE_CREATED_BIT_INDEX,
                                                                      1,
                                                                      0);
                /* Update the received Pipe ID in its alloted slot of Session ID*/
                aSetHciSessionId[PHHCI_ESE_APDU_PIPE_STORAGE_INDEX] = tPipeCreatedNtfParams.bPipeID;

                /*Update the Pipe List used later for Transeive for checking*/
                pHciContext ->aSEPipeList[PHHCI_ESE_APDU_PIPE_LIST_INDEX].bPipeId = tPipeCreatedNtfParams.bPipeID;
                pHciContext ->aSEPipeList[PHHCI_ESE_APDU_PIPE_LIST_INDEX].bGateId = tPipeCreatedNtfParams.bDestGID;
            }
            else if( tPipeCreatedNtfParams.bDestGID == phHciNfc_e_ConnectivityGate)
            {
                /* Update the Session ID Pipe Presence Indicator Byte */
                aSetHciSessionId[PHHCI_PIPE_PRESENCE_INDEX] = SET_BITS8(aSetHciSessionId[PHHCI_PIPE_PRESENCE_INDEX],
                                                                      PHHCI_ESE_CONNECTIVITY_PIPE_CREATED_BIT_INDEX,
                                                                      1,
                                                                      0);
                /* Update the received Pipe ID in its alloted slot of Session ID*/
                aSetHciSessionId[PHHCI_ESE_CONNECTIVITY_PIPE_STORAGE_INDEX] = tPipeCreatedNtfParams.bPipeID;

                /*Update the Pipe List used later for Transeive for checking*/
                pHciContext ->aSEPipeList[PHHCI_ESE_CONN_PIPE_LIST_INDEX].bPipeId = tPipeCreatedNtfParams.bPipeID;
                pHciContext ->aSEPipeList[PHHCI_ESE_CONN_PIPE_LIST_INDEX].bGateId = tPipeCreatedNtfParams.bDestGID;
            }
        }
        else
        {
            /* Pipe Created Ntf from UICC*/
            /* Check the Gate ID to which Pipe Request is received */
            if( tPipeCreatedNtfParams.bDestGID == phHciNfc_e_ConnectivityGate)
            {
                /* Update the Session ID Pipe Presence Indicator Byte */
                aSetHciSessionId[PHHCI_PIPE_PRESENCE_INDEX] = SET_BITS8(aSetHciSessionId[PHHCI_PIPE_PRESENCE_INDEX],
                                                                      PHHCI_UICC_CONNECTIVITY_PIPE_CREATED_BIT_INDEX,
                                                                      1,
                                                                      0);
                /* Update the received Pipe ID in its alloted slot of Session ID*/
                aSetHciSessionId[PHHCI_UICC_CONNECTIVITY_PIPE_STORAGE_INDEX] = tPipeCreatedNtfParams.bPipeID;

                /* Update the Pipe ID and Gate ID to UICC List */
                pHciContext->aUICCPipeList[PHHCI_UICC_CONN_PIPE_LIST_INDEX].bPipeId = tPipeCreatedNtfParams.bPipeID;
                pHciContext->aUICCPipeList[PHHCI_UICC_CONN_PIPE_LIST_INDEX].bGateId = tPipeCreatedNtfParams.bDestGID;
            }
        }

        phOsalNfc_MemCopy(pHciContext->aGetHciSessionId,aSetHciSessionId, PHHCINFC_PIPE_SESSIONID_LEN);
        pHciContext->bCreatePipe = 0x01;
    }
    else
    {
        wIntStatus = phHciNfc_e_RspAnyERegAccessDenied;
    }
    PH_LOG_HCI_FUNC_EXIT();
    return wIntStatus;
}

static NFCSTATUS phHciNfc_ProcessClearAllPipeNotifyCmd(phHciNfc_ReceiveParams_t *pReceivedParams,
                                      pphHciNfc_HciContext_t    pHciContext)
{
    NFCSTATUS wStatus = NFCSTATUS_SUCCESS;
    uint8_t aSetHciSessionId[8];
    PH_LOG_HCI_FUNC_ENTRY();

    if(pReceivedParams->pData[0] != phHciNfc_e_UICCHostID)
    {
        if(0 == GET_BITS8(pHciContext->aGetHciSessionId[PHHCI_PIPE_PRESENCE_INDEX],
                                PHHCI_ESE_APDU_PIPE_CREATED_BIT_INDEX,1))
        {
            /* Read the Session ID to clear the pipe data */
            phOsalNfc_MemCopy(aSetHciSessionId,pHciContext->aGetHciSessionId,PHHCINFC_PIPE_SESSIONID_LEN);
            /* Reset the Pipe Presence bit*/
            aSetHciSessionId[PHHCI_PIPE_PRESENCE_INDEX] = SET_BITS8(aSetHciSessionId[PHHCI_PIPE_PRESENCE_INDEX],
                                                                        PHHCI_ESE_APDU_PIPE_CREATED_BIT_INDEX,
                                                                        1,
                                                                        1);
            /* Reset the APDU Pipe data present in Session ID for eSE*/
            aSetHciSessionId[PHHCI_ESE_APDU_PIPE_STORAGE_INDEX] = PHHCINFC_NO_PIPE_DATA;
            phOsalNfc_MemCopy(pHciContext->aGetHciSessionId,aSetHciSessionId,PHHCINFC_PIPE_SESSIONID_LEN);

            /* Reset the APDU Pipe data present in the List for eSE*/
            pHciContext->aSEPipeList[PHHCI_ESE_APDU_PIPE_LIST_INDEX].bPipeId = PHHCINFC_NO_PIPE_DATA;
            pHciContext->aSEPipeList[PHHCI_ESE_APDU_PIPE_LIST_INDEX].bGateId = PHHCINFC_NO_PIPE_DATA;
        }
        if(0 == GET_BITS8(pHciContext->aGetHciSessionId[PHHCI_PIPE_PRESENCE_INDEX],
                                PHHCI_ESE_CONNECTIVITY_PIPE_CREATED_BIT_INDEX,1))
        {
            /* Read the Session ID to clear the pipe data */
            phOsalNfc_MemCopy(aSetHciSessionId,pHciContext->aGetHciSessionId,PHHCINFC_PIPE_SESSIONID_LEN);
            /* Reset the Pipe Presence bit*/
            aSetHciSessionId[PHHCI_PIPE_PRESENCE_INDEX] = SET_BITS8(aSetHciSessionId[PHHCI_PIPE_PRESENCE_INDEX],
                                                                        PHHCI_ESE_CONNECTIVITY_PIPE_CREATED_BIT_INDEX,
                                                                        1,
                                                                        1);
            /* Reset the Connectivity Pipe data present in Session ID for eSE*/
            aSetHciSessionId[PHHCI_ESE_CONNECTIVITY_PIPE_STORAGE_INDEX] = PHHCINFC_NO_PIPE_DATA;
            phOsalNfc_MemCopy(pHciContext->aGetHciSessionId,aSetHciSessionId,PHHCINFC_PIPE_SESSIONID_LEN);

            /* Reset the Connectivity Pipe data present in the List for eSE*/
            pHciContext->aSEPipeList[PHHCI_ESE_CONN_PIPE_LIST_INDEX].bPipeId = PHHCINFC_NO_PIPE_DATA;
            pHciContext->aSEPipeList[PHHCI_ESE_CONN_PIPE_LIST_INDEX].bGateId = PHHCINFC_NO_PIPE_DATA;
        }
    }
    else
    {
        if(0 == GET_BITS8(pHciContext->aGetHciSessionId[PHHCI_PIPE_PRESENCE_INDEX],
                           PHHCI_UICC_CONNECTIVITY_PIPE_CREATED_BIT_INDEX,1))
        {
            /* Read the Session ID to clear the pipe data */
            phOsalNfc_MemCopy(aSetHciSessionId,pHciContext->aGetHciSessionId,PHHCINFC_PIPE_SESSIONID_LEN);
            /* Reset the Pipe Presence bit*/
            aSetHciSessionId[PHHCI_PIPE_PRESENCE_INDEX] = SET_BITS8(aSetHciSessionId[PHHCI_PIPE_PRESENCE_INDEX],
                                                                        PHHCI_UICC_CONNECTIVITY_PIPE_CREATED_BIT_INDEX,
                                                                        1,
                                                                        1);
            /* Reset the Pipe data present in Session ID for UICC*/
            aSetHciSessionId[PHHCI_UICC_CONNECTIVITY_PIPE_STORAGE_INDEX] = PHHCINFC_NO_PIPE_DATA;
            phOsalNfc_MemCopy(pHciContext->aGetHciSessionId,aSetHciSessionId,PHHCINFC_PIPE_SESSIONID_LEN);

            /* Reset the Pipe data present in th List for UICC*/
            pHciContext->aUICCPipeList[PHHCI_UICC_CONN_PIPE_LIST_INDEX].bPipeId = PHHCINFC_NO_PIPE_DATA;
            pHciContext->aUICCPipeList[PHHCI_UICC_CONN_PIPE_LIST_INDEX].bGateId = PHHCINFC_NO_PIPE_DATA;
        }
    }
    pHciContext->bClearpipes = 0x01;
    PH_LOG_HCI_FUNC_EXIT();
    return wStatus;
}

void
phHciNfc_ReceiveAdminNotifyCmd( void *pContext,NFCSTATUS wStatus, void *pInfo)
{
    uint8_t bSendAnyOk = 0;
    pphHciNfc_HciContext_t    pHciContext = NULL;
    phHciNfc_ReceiveParams_t *pReceivedParams = (phHciNfc_ReceiveParams_t *) pInfo;
    phHciNfc_SendParams_t tSendParams;
    phHciNfc_RspType_t wIntStatus = phHciNfc_e_RspAnyOk;
    PH_LOG_HCI_FUNC_ENTRY();

    /*Check receive is success  */
    if((NULL != pContext) && (NFCSTATUS_SUCCESS == wStatus) && (NULL != pReceivedParams))
    {
        pHciContext = (pphHciNfc_HciContext_t)pContext;
        /* Check notifications sent by the Host Controller */
        switch( pReceivedParams->bIns)
        {
            /* Notification Received for Pipe Creation */
            case phHciNfc_e_AdmNotifyPipeCreated:
            {
                bSendAnyOk = 1;
                wStatus = phHciNfc_ProcessPipeCreateNotifyCmd(pReceivedParams,pHciContext);
                break;
            }
            case phHciNfc_e_AdmNotifyPipeDeleted:
            {
                bSendAnyOk = 1;
                break;
            }
            case phHciNfc_e_AdmClearAllPipe:
            case phHciNfc_e_AdmNotifyAllPipeCleared:
            {
                bSendAnyOk = 1;
                wStatus = phHciNfc_ProcessClearAllPipeNotifyCmd(pReceivedParams,pHciContext);
                break;
            }
            default:
            {
                wIntStatus = phHciNfc_e_RspAnyECmdNotSupported;
                break;
            }
        }

        if(1 == bSendAnyOk)
        {
            /* All the ADMIN command flow on ADMIN Pipe */
            tSendParams.bPipeId = PHHCINFC_PIPE_ADMIN_PIPEID;
            /* Ins value is ANY OK */
            tSendParams.bIns = wIntStatus;
            /* Message Type is Response */
            tSendParams.bMsgType = phHciNfc_e_HcpMsgTypeResponse;
            /* No Data */
            tSendParams.pData = NULL;
            tSendParams.dwLen = 0;
            wStatus = phHciNfc_CoreSend (pHciContext,&tSendParams,&phHciNfc_AnyOkCb,pHciContext);
        }
    }
    PH_LOG_HCI_FUNC_EXIT();
}

void
phHciNfc_ReceiveOpenPipeNotifyCmd(void *pContext,NFCSTATUS wStatus, void *pInfo)
{
    uint8_t bSendAnyOk = 0;
    pphHciNfc_HciContext_t pHciContext = NULL;
    phHciNfc_ReceiveParams_t *pReceivedParams = (phHciNfc_ReceiveParams_t *) pInfo;
    phHciNfc_SendParams_t tSendParams;
    phHciNfc_RspType_t wIntStatus = phHciNfc_e_RspAnyOk;
    uint8_t bData = 0x00;
    phHciNfc_HciRegData_t  tHciRegData;
    PH_LOG_HCI_FUNC_ENTRY();

    if((NULL != pContext) &&(NULL != pReceivedParams))
    {
        pHciContext = (pphHciNfc_HciContext_t)pContext;
        /*Check receive is success  */
        if(NFCSTATUS_SUCCESS == wStatus)
        {
            /* Check notifications sent by the Host Controller */
            /* modified from switch statement to if else to fix Qac/Qmore warnings */
            if(phHciNfc_e_AnyOpenPipe == pReceivedParams->bIns)
            {
                bSendAnyOk = 1;
                /* Unregister for Events when are received over notified pipe */
                tHciRegData.eMsgType = phHciNfc_e_HciMsgTypeCmd;
                tHciRegData.bPipeId = pReceivedParams->bPipeId;
                (void)phHciNfc_UnRegisterCmdRspEvt(pHciContext,
                                &tHciRegData,
                                &phHciNfc_ReceiveOpenPipeNotifyCmd);
            }
            else
            {
                wIntStatus = phHciNfc_e_RspAnyECmdNotSupported;
            }

            if(1 == bSendAnyOk)
            {
                /* All the ADMIN command flow on ADMIN Pipe */
                tSendParams.bPipeId = pReceivedParams->bPipeId; /* Apdu Pipe */
                /* Ins value is ANY OK */
                tSendParams.bIns = wIntStatus;
                /* Message Type is Response */
                tSendParams.bMsgType = phHciNfc_e_HcpMsgTypeResponse;
                /* No of Open Pipes */
                tSendParams.pData = &bData;
                /* DataLength */
                tSendParams.dwLen = 0x01;

                wStatus = phHciNfc_CoreSend (pHciContext,&tSendParams,&phHciNfc_AnyOkCb, pHciContext);
                if((NFCSTATUS_PENDING == wStatus))
                {
                    /* Do not register for events at APDU Gate Pipe */
                    if(pReceivedParams->bPipeId != pHciContext->aGetHciSessionId[PHHCI_ESE_APDU_PIPE_STORAGE_INDEX])
                    {
                        /* Register for Evt for the opened pipe */
                        tHciRegData.eMsgType = phHciNfc_e_HciMsgTypeEvent;
                        tHciRegData.bPipeId = pReceivedParams->bPipeId;
                        (void )phHciNfc_RegisterCmdRspEvt(pHciContext,
                                                &tHciRegData,
                                                &phHciNfc_ProcessEventsOnPipe,
                                                pHciContext);
                    }
                }
            }
        }
    }
    PH_LOG_HCI_FUNC_EXIT();
    return;
}

void phHciNfc_ProcessEventsOnPipe( void *pContext,NFCSTATUS wStatus, void *pInfo)
{
    pphHciNfc_HciContext_t pHciContext = NULL;
    phHciNfc_ReceiveParams_t *pReceivedParams = (phHciNfc_ReceiveParams_t *) pInfo;
    uint16_t bIndex = 0x00;
    uint8_t bTag = 0x00, bLen = 0x00;
    phLibNfc_uSeEvtInfo_t tSeEvtInfo = {0};
    phLibNfc_eSE_EvtType_t eEventType;
    PH_LOG_HCI_FUNC_ENTRY();

    if((NULL != pContext))
    {
        pHciContext = (pphHciNfc_HciContext_t)pContext;
        /*Check receive is success  */
        if(NFCSTATUS_SUCCESS == wStatus)
        {
            /*Check for Transaction events */
            if(pReceivedParams->bIns == PH_LIBNFC_INTERNAL_HCI_TRANSACTION_EVENT)
            {
                while((bIndex+2) < pReceivedParams->wLen)
                {
                    bTag = pReceivedParams->pData[bIndex++];
                    bLen = pReceivedParams->pData[bIndex++];

                    if((bTag == PHLIBNFC_TRANSACTION_AID) && (bLen > 0) && ((bIndex+bLen) <= pReceivedParams->wLen))
                    {
                        tSeEvtInfo.UiccEvtInfo.aid.length = bLen;
                        tSeEvtInfo.UiccEvtInfo.aid.buffer = &pReceivedParams->pData[bIndex];
                    }
                    else if((bTag == PHLIBNFC_TRANSACTION_PARAM) && (bLen > 0) && ((bIndex+bLen) <= pReceivedParams->wLen))
                    {
                        tSeEvtInfo.UiccEvtInfo.param.length = bLen;
                        tSeEvtInfo.UiccEvtInfo.param.buffer = &pReceivedParams->pData[bIndex];
                    }

                    bIndex += bLen;
                }

                if((tSeEvtInfo.UiccEvtInfo.aid.buffer != NULL) && (tSeEvtInfo.UiccEvtInfo.aid.length > 0))
                {
                    eEventType = phLibNfc_eSE_EvtTypeTransaction;
                    (void)phLibNfc_InvokeSeNtfCallback(pContext,(void *)&tSeEvtInfo,wStatus,pReceivedParams->bPipeId,eEventType);
                }
            }
            /*Check for Connectivity events */
            else if(pReceivedParams->bIns == PH_LIBNFC_INTERNAL_HCI_CONNECTIVITY_EVENT)
            {
                tSeEvtInfo.UiccEvtInfo.param.buffer = NULL;
                tSeEvtInfo.UiccEvtInfo.param.length = 0x00;
                tSeEvtInfo.UiccEvtInfo.aid.length = 0x00;
                tSeEvtInfo.UiccEvtInfo.aid.buffer = NULL;
                eEventType = phLibNfc_eSE_EvtConnectivity;
                (void)phLibNfc_InvokeSeNtfCallback(pContext,(void *)&tSeEvtInfo,wStatus,pReceivedParams->bPipeId,eEventType);
            }
        }
    }
    PH_LOG_HCI_FUNC_EXIT();
    return;
}

NFCSTATUS
phHciNfc_GetPipeId(void *pContext, uint8_t bGateType, uint8_t *bPipeId)
{
    NFCSTATUS wStatus = NFCSTATUS_INVALID_PARAMETER;
    pphHciNfc_HciContext_t    pHciContext = NULL;
    uint8_t bIndex =0;
    PH_LOG_HCI_FUNC_ENTRY();

     if((NULL != pContext)&&(NULL != bPipeId))
    {
        pHciContext = (pphHciNfc_HciContext_t)pContext;
        /* Search for the Gate Id in the list  */
        for(bIndex = 0;bIndex < 3;bIndex++)
        {
            /* Check for the Gate Id present in the list */
            if(pHciContext->aSEPipeList[bIndex].bGateId == bGateType)
            {
                /* Return the Pipe for the Gate Id present in the list */
                *bPipeId = pHciContext->aSEPipeList[bIndex].bPipeId;
                wStatus = NFCSTATUS_SUCCESS;
                break;
            }
        }
     }

    PH_LOG_HCI_FUNC_EXIT();
    return wStatus;
}
