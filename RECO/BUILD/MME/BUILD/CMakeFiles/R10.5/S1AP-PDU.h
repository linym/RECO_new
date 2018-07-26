/*
 * Generated by asn1c-0.9.24 (http://lionet.info/asn1c)
 * From ASN.1 module "S1AP-PDU"
 * 	found in "/RECO/SRC/S1AP/MESSAGES/ASN1/R10.5/S1AP-PDU.asn"
 * 	`asn1c -gen-PER`
 */

#ifndef	_S1AP_PDU_H_
#define	_S1AP_PDU_H_


#include <asn_application.h>

/* Including external dependencies */
#include "S1ap-InitiatingMessage.h"
#include "S1ap-SuccessfulOutcome.h"
#include "S1ap-UnsuccessfulOutcome.h"
#include <constr_CHOICE.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Dependencies */
typedef enum S1AP_PDU_PR {
	S1AP_PDU_PR_NOTHING,	/* No components present */
	S1AP_PDU_PR_initiatingMessage,
	S1AP_PDU_PR_successfulOutcome,
	S1AP_PDU_PR_unsuccessfulOutcome,
	/* Extensions may appear below */
	
} S1AP_PDU_PR;

/* S1AP-PDU */
typedef struct S1AP_PDU {
	S1AP_PDU_PR present;
	union S1AP_PDU_u {
		S1ap_InitiatingMessage_t	 initiatingMessage;
		S1ap_SuccessfulOutcome_t	 successfulOutcome;
		S1ap_UnsuccessfulOutcome_t	 unsuccessfulOutcome;
		/*
		 * This type is extensible,
		 * possible extensions are below.
		 */
	} choice;
	
	/* Context for parsing across buffer boundaries */
	asn_struct_ctx_t _asn_ctx;
} S1AP_PDU_t;

/* Implementation */
extern asn_TYPE_descriptor_t asn_DEF_S1AP_PDU;

#ifdef __cplusplus
}
#endif

#endif	/* _S1AP_PDU_H_ */
#include <asn_internal.h>
