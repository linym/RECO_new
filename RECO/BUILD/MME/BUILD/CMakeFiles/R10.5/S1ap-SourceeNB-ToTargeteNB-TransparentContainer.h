/*
 * Generated by asn1c-0.9.24 (http://lionet.info/asn1c)
 * From ASN.1 module "S1AP-IEs"
 * 	found in "/RECO/SRC/S1AP/MESSAGES/ASN1/R10.5/S1AP-IEs.asn"
 * 	`asn1c -gen-PER`
 */

#ifndef	_S1ap_SourceeNB_ToTargeteNB_TransparentContainer_H_
#define	_S1ap_SourceeNB_ToTargeteNB_TransparentContainer_H_


#include <asn_application.h>

/* Including external dependencies */
#include "S1ap-RRC-Container.h"
#include "S1ap-EUTRAN-CGI.h"
#include "S1ap-SubscriberProfileIDforRFP.h"
#include "S1ap-UE-HistoryInformation.h"
#include <constr_SEQUENCE.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct S1ap_E_RABInformationList;
struct S1ap_IE_Extensions;

/* S1ap-SourceeNB-ToTargeteNB-TransparentContainer */
typedef struct S1ap_SourceeNB_ToTargeteNB_TransparentContainer {
	S1ap_RRC_Container_t	 rRC_Container;
	struct S1ap_E_RABInformationList	*e_RABInformationList	/* OPTIONAL */;
	S1ap_EUTRAN_CGI_t	 targetCell_ID;
	S1ap_SubscriberProfileIDforRFP_t	*subscriberProfileIDforRFP	/* OPTIONAL */;
	S1ap_UE_HistoryInformation_t	 uE_HistoryInformation;
	struct S1ap_IE_Extensions	*iE_Extensions	/* OPTIONAL */;
	/*
	 * This type is extensible,
	 * possible extensions are below.
	 */
	
	/* Context for parsing across buffer boundaries */
	asn_struct_ctx_t _asn_ctx;
} S1ap_SourceeNB_ToTargeteNB_TransparentContainer_t;

/* Implementation */
extern asn_TYPE_descriptor_t asn_DEF_S1ap_SourceeNB_ToTargeteNB_TransparentContainer;

#ifdef __cplusplus
}
#endif

/* Referred external types */
#include "S1ap-E-RABInformationList.h"
#include "S1ap-IE-Extensions.h"

#endif	/* _S1ap_SourceeNB_ToTargeteNB_TransparentContainer_H_ */
#include <asn_internal.h>
