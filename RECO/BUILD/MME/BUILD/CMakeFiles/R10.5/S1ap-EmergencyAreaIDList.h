/*
 * Generated by asn1c-0.9.24 (http://lionet.info/asn1c)
 * From ASN.1 module "S1AP-IEs"
 * 	found in "/RECO/SRC/S1AP/MESSAGES/ASN1/R10.5/S1AP-IEs.asn"
 * 	`asn1c -gen-PER`
 */

#ifndef	_S1ap_EmergencyAreaIDList_H_
#define	_S1ap_EmergencyAreaIDList_H_


#include <asn_application.h>

/* Including external dependencies */
#include "S1ap-EmergencyAreaID.h"
#include <asn_SEQUENCE_OF.h>
#include <constr_SEQUENCE_OF.h>

#ifdef __cplusplus
extern "C" {
#endif

/* S1ap-EmergencyAreaIDList */
typedef struct S1ap_EmergencyAreaIDList {
	A_SEQUENCE_OF(S1ap_EmergencyAreaID_t) list;
	
	/* Context for parsing across buffer boundaries */
	asn_struct_ctx_t _asn_ctx;
} S1ap_EmergencyAreaIDList_t;

/* Implementation */
extern asn_TYPE_descriptor_t asn_DEF_S1ap_EmergencyAreaIDList;

#ifdef __cplusplus
}
#endif

#endif	/* _S1ap_EmergencyAreaIDList_H_ */
#include <asn_internal.h>
