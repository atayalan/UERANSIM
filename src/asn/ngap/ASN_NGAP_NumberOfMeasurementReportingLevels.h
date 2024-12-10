/*
 * Generated by asn1c-0.9.29 (http://lionet.info/asn1c)
 * From ASN.1 module "NGAP-IEs"
 * 	found in "NGAP-IEs.asn"
 * 	`asn1c -pdu=all -fcompound-names -findirect-choice -fno-include-deps -no-gen-OER -gen-PER -no-gen-example -D ngap`
 */

#ifndef	_ASN_NGAP_NumberOfMeasurementReportingLevels_H_
#define	_ASN_NGAP_NumberOfMeasurementReportingLevels_H_


#include <asn_application.h>

/* Including external dependencies */
#include <NativeEnumerated.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Dependencies */
typedef enum ASN_NGAP_NumberOfMeasurementReportingLevels {
	ASN_NGAP_NumberOfMeasurementReportingLevels_n2	= 0,
	ASN_NGAP_NumberOfMeasurementReportingLevels_n3	= 1,
	ASN_NGAP_NumberOfMeasurementReportingLevels_n4	= 2,
	ASN_NGAP_NumberOfMeasurementReportingLevels_n5	= 3,
	ASN_NGAP_NumberOfMeasurementReportingLevels_n10	= 4,
	/*
	 * Enumeration is extensible
	 */
	ASN_NGAP_NumberOfMeasurementReportingLevels_n0	= 5
} e_ASN_NGAP_NumberOfMeasurementReportingLevels;

/* ASN_NGAP_NumberOfMeasurementReportingLevels */
typedef long	 ASN_NGAP_NumberOfMeasurementReportingLevels_t;

/* Implementation */
extern asn_per_constraints_t asn_PER_type_ASN_NGAP_NumberOfMeasurementReportingLevels_constr_1;
extern asn_TYPE_descriptor_t asn_DEF_ASN_NGAP_NumberOfMeasurementReportingLevels;
extern const asn_INTEGER_specifics_t asn_SPC_ASN_NGAP_NumberOfMeasurementReportingLevels_specs_1;
asn_struct_free_f ASN_NGAP_NumberOfMeasurementReportingLevels_free;
asn_struct_print_f ASN_NGAP_NumberOfMeasurementReportingLevels_print;
asn_constr_check_f ASN_NGAP_NumberOfMeasurementReportingLevels_constraint;
ber_type_decoder_f ASN_NGAP_NumberOfMeasurementReportingLevels_decode_ber;
der_type_encoder_f ASN_NGAP_NumberOfMeasurementReportingLevels_encode_der;
xer_type_decoder_f ASN_NGAP_NumberOfMeasurementReportingLevels_decode_xer;
xer_type_encoder_f ASN_NGAP_NumberOfMeasurementReportingLevels_encode_xer;
per_type_decoder_f ASN_NGAP_NumberOfMeasurementReportingLevels_decode_uper;
per_type_encoder_f ASN_NGAP_NumberOfMeasurementReportingLevels_encode_uper;
per_type_decoder_f ASN_NGAP_NumberOfMeasurementReportingLevels_decode_aper;
per_type_encoder_f ASN_NGAP_NumberOfMeasurementReportingLevels_encode_aper;

#ifdef __cplusplus
}
#endif

#endif	/* _ASN_NGAP_NumberOfMeasurementReportingLevels_H_ */
#include <asn_internal.h>
