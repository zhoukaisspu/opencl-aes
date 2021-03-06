// Add you host code
#include <CL/opencl.h>
#include <iostream>
#include "Util.h"
#include "OCL_Device.h"
#include "aes_core.h"
#include <ctime>

int main(int argc, char** argv)
{
	unsigned int MemorySize = 250; // create 250M data
	std::cout << argv[0] << std::endl;
	cl_context context = NULL;
	cl_command_queue commandQueue = NULL;
	cl_device_id device = NULL;
	cl_program aes_program = NULL;
	cl_kernel aes_kernel = NULL;
	cl_int errNum;
	context = ocl::creteContext();
	ocl::DisplayContextInfo(context);
	commandQueue = ocl::createCommandQueue(context, &device);
	aes_program = ocl::CreateProgram(context, device, "aes_kernel.cl");
	aes_kernel = clCreateKernel(aes_program, "AES_ECB_Encrypt", &errNum);
	ocl::CHECK_OPENCL_ERROR(errNum);
	unsigned char plainText[] = {	0xa3,0xc5,0x08,0x08,
									0x78,0xa4,0xff,0xd3,
									0x00,0xff,0x36,0x36,
									0x28,0x5f,0x01,0x02 };
	unsigned char cipherKey[] = {	0x36,0x8a,0xc0,0xf4,
									0xed,0xcf,0x76,0xa6,
									0x08,0xa3,0xb6,0x78,
									0x31,0x31,0x27,0x6e};
	unsigned int *roundkey = new unsigned int[44];
	aes::AesExpandEncryptKey(roundkey, cipherKey, 16);

	unsigned long textLen = sizeof(plainText);
	unsigned int BlockSize = 256 * 16;
	unsigned long memLen = (unsigned long)((MemorySize * 1024 * 1024 + BlockSize - 1) / BlockSize) * BlockSize;
	unsigned char *aes = new unsigned char[memLen];
	unsigned char *destAes = new unsigned char[memLen];
	size_t globalSize[1] = { memLen / 16 };
	size_t localSize[1] = { 256 };
	for (unsigned long i = 0; i < memLen; ++i) {
		int value = (i / 16) % 3;
		if (value == 0)
			aes[i] = plainText[i%textLen];
		else if (value == 1)
			aes[i] = 0x00;
		else
			aes[i] = 0x01;
 	}
	std::clock_t time = clock();
	cl_mem srcMem = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
		sizeof(unsigned char)*memLen, aes, NULL);
	cl_mem keyMem = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
		sizeof(unsigned int) * 44, roundkey, NULL);
	cl_mem destMem = clCreateBuffer(context, CL_MEM_READ_WRITE,
		sizeof(unsigned char)*memLen, NULL, NULL);
	if (srcMem == NULL) {
		std::cout << "create  src memory object failed" << std::endl;
	}
	if (keyMem == NULL) {
		std::cout << "create  KEY memory object failed" << std::endl;
	}
	if (destMem == NULL) {
		std::cout << "create  dest memory object failed" << std::endl;
	}
	errNum = clSetKernelArg(aes_kernel, 0, sizeof(cl_mem), &srcMem);
	ocl::CHECK_OPENCL_ERROR(errNum);
	errNum = clSetKernelArg(aes_kernel, 1, sizeof(cl_mem), &destMem);
	ocl::CHECK_OPENCL_ERROR(errNum);
	errNum = clSetKernelArg(aes_kernel, 2, sizeof(cl_mem), &keyMem);
	ocl::CHECK_OPENCL_ERROR(errNum);
	errNum = clSetKernelArg(aes_kernel, 3, sizeof(cl_uint) * 44, NULL);
	ocl::CHECK_OPENCL_ERROR(errNum);
	errNum = clSetKernelArg(aes_kernel, 4, sizeof(cl_uint) * 1024, NULL);
	ocl::CHECK_OPENCL_ERROR(errNum);
	errNum = clEnqueueNDRangeKernel(commandQueue, aes_kernel, 1, NULL, globalSize,localSize,0,
		NULL,NULL);
	ocl::CHECK_OPENCL_ERROR(errNum);
	errNum = clFinish(commandQueue);
	ocl::CHECK_OPENCL_ERROR(errNum);
	errNum = clEnqueueReadBuffer(commandQueue, destMem, CL_TRUE, 0, sizeof(unsigned char)*memLen, 
		destAes, 0, NULL, NULL);
	ocl::CHECK_OPENCL_ERROR(errNum);
	time = clock() - time;
	double totalTime = (double)time / CLOCKS_PER_SEC;
	std::cout << std::dec;
	std::cout << "crypto time: " << totalTime <<std::endl;
	std::cout << "speed:" << (double)(memLen / totalTime / 1024.0f / 1024.0f) << "  MBytes/s" << std::endl;;
	std::cout << std::hex;
	for (int j = 0; j < 20; ++j) { // just for test
		std::cout << std::dec;
		std::cout << "[" << j << "]: ";
		std::cout << std::hex;
		for (unsigned i = 0; i < textLen; ++i) {
			std::cout << (int)destAes[i+j*16] << " ";
		}
		std::cout << "\n";
	}

	std::cout << "Start Decrypt..." << std::endl;
	int descryret = aes::AesExpandDecryptKey(roundkey, cipherKey, 16);
	if (descryret != 0)
		std::cout << "Descrypt key expand failure" << std::endl;
	cl_kernel aes_desKernel = clCreateKernel(aes_program, "AES_ECB_Decrypt", &errNum);
	ocl::CHECK_OPENCL_ERROR(errNum);
	time = clock();
	errNum = clEnqueueWriteBuffer(commandQueue, srcMem, CL_TRUE, 0, 
		sizeof(unsigned char)*memLen, destAes, 0, NULL, NULL);
	ocl::CHECK_OPENCL_ERROR(errNum);
	errNum = clEnqueueWriteBuffer(commandQueue, keyMem, CL_TRUE, 0, 
		sizeof(unsigned int) * 44, roundkey, 0, NULL, NULL);
	ocl::CHECK_OPENCL_ERROR(errNum);
	errNum = clSetKernelArg(aes_desKernel, 0, sizeof(cl_mem), &srcMem);
	ocl::CHECK_OPENCL_ERROR(errNum);
	errNum = clSetKernelArg(aes_desKernel, 1, sizeof(cl_mem), &destMem);
	ocl::CHECK_OPENCL_ERROR(errNum);
	errNum = clSetKernelArg(aes_desKernel, 2, sizeof(cl_mem), &keyMem);
	ocl::CHECK_OPENCL_ERROR(errNum);
	errNum = clSetKernelArg(aes_desKernel, 3, sizeof(cl_uint) * 44, NULL);
	ocl::CHECK_OPENCL_ERROR(errNum);
	errNum = clSetKernelArg(aes_desKernel, 4, sizeof(cl_uint) * 1024, NULL);
	ocl::CHECK_OPENCL_ERROR(errNum);
	errNum = clEnqueueNDRangeKernel(commandQueue, aes_desKernel, 1, NULL, globalSize, localSize, 0,
		NULL, NULL);
	ocl::CHECK_OPENCL_ERROR(errNum);
	errNum = clFinish(commandQueue);
	ocl::CHECK_OPENCL_ERROR(errNum);
	errNum = clEnqueueReadBuffer(commandQueue, destMem, CL_TRUE, 0, sizeof(unsigned char)*memLen,
		aes, 0, NULL, NULL);
	ocl::CHECK_OPENCL_ERROR(errNum);
	time = clock() - time;
	double DecryptTime = (double)time / CLOCKS_PER_SEC;
	std::cout << std::dec;
	std::cout << "Decrypt time: " << DecryptTime << std::endl;
	std::cout << "Decrypt speed: " << (double)(memLen / DecryptTime / 1024.0f / 1024.0f) << "  MBytes/s" << std::endl;;
	std::cout << std::hex;
	for (int j = 0; j < 20; ++j) { // just for test
		std::cout << std::dec;
		std::cout << "[" << j << "]: ";
		std::cout << std::hex;
		for (unsigned i = 0; i < textLen; ++i) {
			std::cout << (int)aes[i + j * 16] << " ";
		}
		std::cout << "\n";
	}
	delete[] roundkey;
	delete[] aes;
	delete[] destAes;
	return 0;
}