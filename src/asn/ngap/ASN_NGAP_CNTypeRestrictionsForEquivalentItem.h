/*
 * Generated by asn1c-0.9.29 (http://lionet.info/asn1c)
 * From ASN.1 module "NGAP-IEs"
 * 	found in "NGAP-IEs.asn"
 * 	`asn1c -pdu=all -fcompound-names -findirect-choice -fno-include-deps -no-gen-OER -gen-PER -no-gen-example -D ngap`
 */

#ifndef	_ASN_NGAP_CNTypeRestrictionsForEquivalentItem_H_
#define	_ASN_NGAP_CNTypeRestrictionsForEquivalentItem_H_


#include <asn_application.h>

/* Including external dependencies */
#include "ASN_NGAP_PLMNIdentity.h"
#include <NativeEnumerated.h>
#include <constr_SEQUENCE.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Dependencies */
typedef enum ASN_NGAP_CNTypeRestrictionsForEquivalentItem__cn_Type {
	ASN_NGAP_CNTypeRestrictionsForEquivalentItem__cn_Type_epc_forbidden	= 0,
	ASN_NGAP_CNTypeRestrictionsForEquivalentItem__cn_Type_fiveGC_forbidden	= 1
	/*
	 * Enumeration is extensible
	 */
} e_ASN_NGAP_CNTypeRestrictionsForEquivalentItem__cn_Type;

/* Forward declarations */
struct ASN_NGAP_ProtocolExtensionContainer;

/* ASN_NGAP_CNTypeRestrictionsForEquivalentItem */
typedef struct ASN_NGAP_CNTypeRestrictionsForEquivalentItem {
	ASN_NGAP_PLMNIdentity_t	 plmnIdentity;
	long	 cn_Type;
	struct ASN_NGAP_ProtocolExtensionContainer	*iE_Extensions;	/* OPTIONAL */
	/*
	 * This type is extensible,
	 * possible extensions are below.
	 */
	
	/* Context for parsing across buffer boundaries */
	asn_struct_ctx_t _asn_ctx;
} ASN_NGAP_CNTypeRestrictionsForEquivalentItem_t;

/* Implementation */
/* extern asn_TYPE_descriptor_t asn_DEF_ASN_NGAP_cn_Type_3;	// (Use -fall-defs-global to expose) */
extern asn_TYPE_descriptor_t asn_DEF_ASN_NGAP_CNTypeRestrictionsForEquivalentItem;
extern asn_SEQUENCE_specifics_t asn_SPC_ASN_NGAP_CNTypeRestrictionsForEquivalentItem_specs_1;
extern asn_TYPE_member_t asn_MBR_ASN_NGAP_CNTypeRestrictionsForEquivalentItem_1[3];

#ifdef __cplusplus
}
#endif

#endif	/* _ASN_NGAP_CNTypeRestrictionsForEquivalentItem_H_ */
#include <asn_internal.h>
