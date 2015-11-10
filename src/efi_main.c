
// Copyright 2015 Shin'ichi Ichikawa. Released under the MIT license.

#include <efi.h>
#include <efilib.h>
#include <string.h>
#include <stdio.h>
#include <intrin.h>

#define CHECK_BIT_CPU_INFO(cpu_info, reg_index, lshift) ((cpu_info)[(reg_index)] & (1 << (lshift)))
#define GET_CPU_INFO(cpu_info, reg_index, lshift) \
	(CHECK_BIT_CPU_INFO((cpu_info), (reg_index), (lshift)) ? L"enable" : L"disable")

typedef enum CPU_INFO_ {
	CPU_INFO_EAX = 0,
	CPU_INFO_EBX,
	CPU_INFO_ECX,
	CPU_INFO_EDX,
}CPU_INFO;

void reset_system(EFI_STATUS status);
void error_print(CHAR16* msg);
EFI_FILE* open_print_info(CHAR16* path);
void convert_to_ascii(char* ascii, CHAR16* wide);
void write_print_info1(EFI_FILE* efi_file, CHAR16* fmt, CHAR16* param);
void write_print_info2(EFI_FILE* efi_file, CHAR16* fmt, UINT32 param);
void write_print_info3(EFI_FILE* efi_file, CHAR16* fmt, UINT32 param1, UINT32 param2);
void close_print_info(EFI_FILE* efi_file);

EFI_STATUS efi_main(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE* system_table)
{
	InitializeLib(image_handle, system_table);

	EFI_FILE* efi_file = open_print_info(L"\\result.txt");

	write_print_info1(efi_file, L"Firmware Vendor: %s", system_table->FirmwareVendor);
	write_print_info2(efi_file, L"Firmware Revision: 0x%x", system_table->FirmwareRevision);

	UINT32 rev_1 = (system_table->Hdr.Revision & 0xffff0000) >> 16;
	UINT32 rev_2 = system_table->Hdr.Revision & 0x0000ffff;
	write_print_info3(efi_file, L"UEFI Version: %d.%d", rev_1, rev_2);

	// EAX, EBX, ECX and EDX
	int cpu_info[4] = { 0 };

	__cpuid(cpu_info, 0x07);

	write_print_info1(efi_file, L"SGX: %s", GET_CPU_INFO(cpu_info, CPU_INFO_EBX, 2));
	write_print_info1(efi_file, L"SMEP: %s", GET_CPU_INFO(cpu_info, CPU_INFO_EBX, 7));
	write_print_info1(efi_file, L"INVPCID: %s", GET_CPU_INFO(cpu_info, CPU_INFO_EBX, 10));
	write_print_info1(efi_file, L"MPX: %s", GET_CPU_INFO(cpu_info, CPU_INFO_EBX, 14));
	write_print_info1(efi_file, L"SMAP: %s", GET_CPU_INFO(cpu_info, CPU_INFO_EBX, 20));
	write_print_info1(efi_file, L"CLFLUSHOPT: %s", GET_CPU_INFO(cpu_info, CPU_INFO_EBX, 23));

	write_print_info1(efi_file, L"PREFETCHWT1: %s", GET_CPU_INFO(cpu_info, CPU_INFO_ECX, 1));
	write_print_info1(efi_file, L"PKU: %s", GET_CPU_INFO(cpu_info, CPU_INFO_ECX, 3));
	write_print_info1(efi_file, L"OSPKE: %s", GET_CPU_INFO(cpu_info, CPU_INFO_ECX, 4));

	close_print_info(efi_file);

	reset_system(EFI_SUCCESS);

	return EFI_SUCCESS;
}

void reset_system(EFI_STATUS status)
{
	EFI_STATUS local_status = EFI_SUCCESS;

	do {
		EFI_INPUT_KEY key;
		local_status = ST->ConIn->ReadKeyStroke(ST->ConIn, &key);
	} while (EFI_SUCCESS != local_status);

	RT->ResetSystem(EfiResetCold, status, 0, NULL);
}

void error_print(CHAR16* msg)
{
	Print(msg);

	reset_system(EFI_SUCCESS);
}

EFI_FILE* open_print_info(CHAR16* path)
{
	EFI_FILE_IO_INTERFACE* efi_simple_file_system = NULL;
	EFI_FILE* efi_file_root = NULL;
	EFI_FILE* efi_file = NULL;

	EFI_STATUS status = BS->LocateProtocol(
		&FileSystemProtocol,
		NULL,
		&efi_simple_file_system
	);

	if (EFI_ERROR(status)){

		error_print(L"LocateProtocol() FileSystemProtocol failed.\n");
	}

	status = efi_simple_file_system->OpenVolume(
		efi_simple_file_system, &efi_file_root
	);

	if (EFI_ERROR(status)){

		error_print(L"OpenVolume() failed.\n");
	}

	status = efi_file_root->Open(
		efi_file_root, &efi_file, path,
		EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE |
		EFI_FILE_MODE_CREATE, EFI_FILE_ARCHIVE
	);

	if (EFI_ERROR(status)){

		error_print(L"Open() failed.\n");
	}

	status = efi_file_root->Close(efi_file_root);

	if (EFI_ERROR(status)){

		error_print(L"Close() failed.\n");
	}

	return efi_file;
}

void convert_to_ascii(char* ascii, CHAR16* wide)
{
	UINTN size = StrLen(wide);

	for (UINTN i = 0; size > i; ++i){

		ascii[i] = (char)(wide[i] & 0xff);
	}

	ascii[size] = '\0';
}

void write_print_info1(EFI_FILE* efi_file, CHAR16* fmt, CHAR16* param)
{
	if (efi_file && fmt && param){

		CHAR16 buffer[256];
		SPrint(buffer, sizeof(buffer), fmt, param);
		StrCat(buffer, L"\n");

		Print(buffer);

		SPrint(buffer, sizeof(buffer), fmt, param);
		StrCat(buffer, L"\r\n");

		char buffer_a[256];
		convert_to_ascii(buffer_a, buffer);

		UINTN size = (UINTN)strlen(buffer_a);

		EFI_STATUS status = efi_file->Write(efi_file, &size, buffer_a);

		if (EFI_ERROR(status)){

			error_print(L"Write() failed.\n");
		}
	}
}

void write_print_info2(EFI_FILE* efi_file, CHAR16* fmt, UINT32 param)
{
	if (efi_file && fmt){

		CHAR16 buffer[256];
		SPrint(buffer, sizeof(buffer), fmt, param);
		StrCat(buffer, L"\n");

		Print(buffer);

		char fmt_a[256];
		convert_to_ascii(fmt_a, fmt);

		char buffer_a[256];
		sprintf(buffer_a, fmt_a, param);
		strcat(buffer_a, "\r\n");

		UINTN size = (UINTN)strlen(buffer_a);

		EFI_STATUS status = efi_file->Write(efi_file, &size, buffer_a);

		if (EFI_ERROR(status)){

			error_print(L"Write() failed.\n");
		}
	}
}

void write_print_info3(EFI_FILE* efi_file, CHAR16* fmt, UINT32 param1, UINT32 param2)
{
	if (efi_file && fmt){

		CHAR16 buffer[256];
		SPrint(buffer, sizeof(buffer), fmt, param1, param2);
		StrCat(buffer, L"\n");

		Print(buffer);

		char fmt_a[256];
		convert_to_ascii(fmt_a, fmt);

		char buffer_a[256];
		sprintf(buffer_a, fmt_a, param1, param2);
		strcat(buffer_a, "\r\n");

		UINTN size = (UINTN)strlen(buffer_a);

		EFI_STATUS status = efi_file->Write(efi_file, &size, buffer_a);

		if (EFI_ERROR(status)){

			error_print(L"Write() failed.\n");
		}
	}
}

void close_print_info(EFI_FILE* efi_file)
{
	if (efi_file){

		EFI_STATUS status = efi_file->Close(efi_file);

		if (EFI_ERROR(status)){

			error_print(L"Close() failed.\n");
		}
	}
}

