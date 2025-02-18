/*
 * Generated by asn1c-0.9.29 (http://lionet.info/asn1c)
 * From ASN.1 module "NGAP-IEs"
 * 	found in "NGAP-IEs.asn"
 * 	`asn1c -pdu=all -fcompound-names -findirect-choice -fno-include-deps -no-gen-OER -gen-PER -no-gen-example -D ngap`
 */

#ifndef	_ASN_NGAP_MBS_DistributionSetupResponseTransfer_H_
#define	_ASN_NGAP_MBS_DistributionSetupResponseTransfer_H_


#include <asn_application.h>

/* Including external dependencies */
#include "ASN_NGAP_MBS-SessionID.h"
#include "ASN_NGAP_MBS-AreaSessionID.h"
#include "ASN_NGAP_MBS-QoSFlowsToBeSetupList.h"
#include "ASN_NGAP_MBSSessionStatus.h"
#include <constr_SEQUENCE.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct ASN_NGAP_SharedNGU_MulticastTNLInformation;
struct ASN_NGAP_MBS_ServiceArea;
struct ASN_NGAP_ProtocolExtensionContainer;

/* ASN_NGAP_MBS-DistributionSetupResponseTransfer */
typedef struct ASN_NGAP_MBS_DistributionSetupResponseTransfer {
	ASN_NGAP_MBS_SessionID_t	 mBS_SessionID;
	ASN_NGAP_MBS_AreaSessionID_t	*mBS_AreaSessionID;	/* OPTIONAL */
	struct ASN_NGAP_SharedNGU_MulticastTNLInformation	*sharedNGU_MulticastTNLInformation;	/* OPTIONAL */
	ASN_NGAP_MBS_QoSFlowsToBeSetupList_t	 mBS_QoSFlowsToBeSetupList;
	ASN_NGAP_MBSSessionStatus_t	 mBSSessionStatus;
	struct ASN_NGAP_MBS_ServiceArea	*mBS_ServiceArea;	/* OPTIONAL */
	struct ASN_NGAP_ProtocolExtensionContainer	*iE_Extensions;	/* OPTIONAL */
	/*
	 * This type is extensible,
	 * possible extensions are below.
	 */
	
	/* Context for parsing across buffer boundaries */
	asn_struct_ctx_t _asn_ctx;
} ASN_NGAP_MBS_DistributionSetupResponseTransfer_t;

/* Implementation */
extern asn_TYPE_descriptor_t asn_DEF_ASN_NGAP_MBS_DistributionSetupResponseTransfer;

#ifdef __cplusplus
}
#endif

#endif	/* _ASN_NGAP_MBS_DistributionSetupResponseTransfer_H_ */
#include <asn_internal.h>
