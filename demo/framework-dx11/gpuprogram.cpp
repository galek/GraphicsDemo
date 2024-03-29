/*
 * Copyright (c) 2014 Roman Kuznetsov 
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "stdafx.h"
#include "gpuprogram.h"

#include <d3dcompiler.h>

static const char* VS_MODEL = "vs_5_0";
static const char* PS_MODEL = "ps_5_0";
static const char* GS_MODEL = "gs_5_0";
static const char* HS_MODEL = "hs_5_0";
static const char* DS_MODEL = "ds_5_0";
static const char* CS_MODEL = "cs_5_0";
static const char* DEF_FUNC = "main";

namespace framework
{

const char* getModelByType(ShaderType shaderType)
{
	switch (shaderType)
	{
		case VERTEX_SHADER: return VS_MODEL;
		case HULL_SHADER: return HS_MODEL;
		case DOMAIN_SHADER: return DS_MODEL;
		case GEOMETRY_SHADER: return GS_MODEL;
		case PIXEL_SHADER: return PS_MODEL;
		case COMPUTE_SHADER: return CS_MODEL;
	}
	return 0;
}

int getTypeByExt(const std::list<std::string>& exts)
{
#define FIND_EXT(str) std::find(exts.cbegin(), exts.cend(), str) != exts.cend()
	if (FIND_EXT("vsh") || FIND_EXT("vs")) return VERTEX_SHADER;
	if (FIND_EXT("hsh") || FIND_EXT("hs")) return HULL_SHADER;
	if (FIND_EXT("dsh") || FIND_EXT("ds")) return DOMAIN_SHADER;
	if (FIND_EXT("gsh") || FIND_EXT("gs")) return GEOMETRY_SHADER;
	if (FIND_EXT("psh") || FIND_EXT("ps")) return PIXEL_SHADER;
	if (FIND_EXT("csh") || FIND_EXT("cs")) return COMPUTE_SHADER;
#undef FIND_EXT
	return -1;
}

template<typename T>
unsigned int sizeByMask(unsigned char mask)
{
	if (mask == 1) return sizeof(T);
	else if (mask == 3) return sizeof(T) * 2;
	else if (mask == 7) return sizeof(T) * 3;
	else if (mask == 15) return sizeof(T) * 4;
	else if (mask == 31) return sizeof(T) * 5;
	else if (mask == 63) return sizeof(T) * 6;
	else if (mask == 127) return sizeof(T) * 7;
	else if (mask == 255) return sizeof(T) * 8;
	return 0;
}

DXGI_FORMAT getComponentFormat(D3D_REGISTER_COMPONENT_TYPE type, unsigned char mask)
{
	if (type == D3D_REGISTER_COMPONENT_UINT32)
	{
		unsigned int sz = sizeByMask<unsigned int>(mask);
		if (sz == 4) return DXGI_FORMAT_R32_UINT;
		else if (sz == 8) return DXGI_FORMAT_R32G32_UINT;
		else if (sz == 12) return DXGI_FORMAT_R32G32B32_UINT;
		else if (sz == 16) return DXGI_FORMAT_R32G32B32A32_UINT;
	}
	else if (type == D3D_REGISTER_COMPONENT_SINT32)
	{
		unsigned int sz = sizeByMask<int>(mask);
		if (sz == 4) return DXGI_FORMAT_R32_SINT;
		else if (sz == 8) return DXGI_FORMAT_R32G32_SINT;
		else if (sz == 12) return DXGI_FORMAT_R32G32B32_SINT;
		else if (sz == 16) return DXGI_FORMAT_R32G32B32A32_SINT;
	}
	else if (type == D3D_REGISTER_COMPONENT_FLOAT32)
	{
		unsigned int sz = sizeByMask<float>(mask);
		if (sz == 4) return DXGI_FORMAT_R32_FLOAT;
		else if (sz == 8) return DXGI_FORMAT_R32G32_FLOAT;
		else if (sz == 12) return DXGI_FORMAT_R32G32B32_FLOAT;
		else if (sz == 16) return DXGI_FORMAT_R32G32B32A32_FLOAT;
	}
	return DXGI_FORMAT_UNKNOWN;
}

class GpuProgramInclude : public ID3DInclude
{
public:
	GpuProgramInclude(const std::string& shaderPath) : m_shaderPath(shaderPath){}

	HRESULT __stdcall Open(D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes)
	{
		std::string path = m_shaderPath + pFileName;
		std::ifstream includeFile(path.c_str(), std::ios::in | std::ios::binary | std::ios::ate);

		if (includeFile.is_open()) 
		{
			unsigned int fileSize = (unsigned int)includeFile.tellg();
			char* buf = new char[fileSize];
			includeFile.seekg(0, std::ios::beg);
			includeFile.read(buf, fileSize);
			includeFile.close();
			*ppData = buf;
			*pBytes = fileSize;
		}
		else
		{
			return E_FAIL;
		}
		return S_OK;
	}
	HRESULT __stdcall Close(LPCVOID pData)
	{
		char* buf = (char*)pData;
		delete[] buf;
		return S_OK;
	}

private:
	std::string m_shaderPath;
};

GpuProgram::GpuProgram() : 
	m_vertexShader(0),
	m_hullShader(0),
	m_domainShader(0),
	m_geometryShader(0),
	m_pixelShader(0),
	m_computeShader(0),
	m_id(-1)
{
	m_data.resize(SHADERS_COUNT);
	m_inputLayoutInfoBase.reserve(16);
}

GpuProgram::~GpuProgram()
{
	destroy();
}

void GpuProgram::destroy()
{
	m_inputLayout.clear();
	m_inputLayoutStringsPool.clear();
	m_inputLayoutInfoBase.clear();

	for (size_t i = 0; i < m_data.size(); i++)
	{
		if (m_data[i].compiledShader != 0)
		{
			m_data[i].compiledShader->Release();
			m_data[i].compiledShader = 0;
		}
		m_data[i].m_resources.clear();
	}

	if (m_vertexShader != 0) { m_vertexShader->Release();  m_vertexShader = 0; }
	if (m_hullShader != 0) { m_hullShader->Release();  m_hullShader = 0; }
	if (m_domainShader != 0) { m_domainShader->Release();  m_domainShader = 0; }
	if (m_geometryShader != 0) { m_geometryShader->Release();  m_geometryShader = 0; }
	if (m_pixelShader != 0) { m_pixelShader->Release();  m_pixelShader = 0; }
	if (m_computeShader != 0) { m_computeShader->Release();  m_computeShader = 0; }

	m_id = -1;
}

void GpuProgram::addShader(const std::string& fileName, const std::string& mainFunc)
{
	int shaderType = getTypeByExt(utils::Utils::getExtentions(fileName));
	if (shaderType < 0)
	{
		utils::Logger::toLogWithFormat("Error: could not add shader '%s'. The reason: shader type is undefined.\n", fileName.c_str());
		return;
	}
	m_data[shaderType].filename = fileName;
	m_data[shaderType].mainFunction = mainFunc;
}

bool GpuProgram::init(bool autoInputLayout)
{
	const Device& device = Application::instance()->getDevice();

	destroy();

	bool noShaders = true;
	std::for_each(m_data.begin(), m_data.end(), [&](const ShaderData& dat)
	{
		if (dat.filename.empty()) noShaders = false;
	});
	if (noShaders)
	{
		utils::Logger::toLog("Error: could not initialize gpu program. The reason: no any shader added.\n");
		return false;
	}

	// compile
	bool result = true;
	for (size_t i = 0; i < m_data.size(); i++)
	{
		if (!m_data[i].filename.empty())
		{
			auto compiledShader = compileShader((ShaderType)i, m_data[i].filename, m_data[i].mainFunction);
			result &= (compiledShader != 0);
			if (result)
			{
				result &= createShaderByType(device, (ShaderType)i, compiledShader);
				m_data[i].compiledShader = compiledShader;
			}
			if (!result) break;
		}
	}
	if (!result)
	{
		destroy();
		return false;
	}

	// reflection
	if (!reflectShaders(device, autoInputLayout))
	{
		utils::Logger::toLog("Error: could not execute shaders reflection.\n");
		destroy();
		return false;
	}

	if (isValid()) 
	{
		m_id = generateId();
		initDestroyable();
	}
	return isValid();
}

bool GpuProgram::isValid() const
{
	return m_vertexShader != 0 || m_hullShader != 0 || m_domainShader != 0 || 
		   m_geometryShader != 0 || m_pixelShader != 0 || m_computeShader != 0;
}

ID3DBlob* GpuProgram::compileShader(ShaderType shaderType, const std::string& filename, const std::string& function, const D3D_SHADER_MACRO* defines)
{
	if (!utils::Utils::exists(filename))
	{
		utils::Logger::toLogWithFormat("Error: could not find file '%s'.\n", filename.c_str());
		return 0;
	}

	std::string sourceStr;
	utils::Utils::readFileToString(filename, sourceStr);
	if (sourceStr.empty())
	{
		utils::Logger::toLogWithFormat("Error: could not read file '%s'.\n", filename.c_str());
		return false;
	}

	std::string mainFunc = function;
	if (mainFunc.empty())
	{
		mainFunc = DEF_FUNC;
	}
	
	HRESULT hr = S_OK;
	ID3DBlob* compiledShader = 0;
	ID3DBlob* errorMessages = 0;

	UINT flags = D3DCOMPILE_PACK_MATRIX_ROW_MAJOR | D3DCOMPILE_SKIP_OPTIMIZATION;
	if (Application::instance()->isDebugEnabled())
	{
		flags |= D3DCOMPILE_DEBUG;
	}
	
	std::string shaderPath = utils::Utils::getPath(filename);
	std::unique_ptr<GpuProgramInclude> includeHandler(new GpuProgramInclude(shaderPath));

	hr = D3DCompile(sourceStr.c_str(), sourceStr.length(), nullptr, defines, includeHandler.get(),
		mainFunc.c_str(), getModelByType(shaderType), flags, 0,
		&compiledShader, &errorMessages);

	if (hr != S_OK)
	{
		utils::Logger::toLogWithFormat("Error: could not compile shader '%s'. The reason: ", filename.c_str());
		if (errorMessages != nullptr)
		{
			LPVOID compileErrors = errorMessages->GetBufferPointer();
			const char* message = (const char*)compileErrors;
			utils::Logger::toLogWithFormat("%s.\n", message);
		}
		else
		{
			utils::Logger::toLog("undefined.\n");
		}

		if (compiledShader != 0) { compiledShader->Release(); }
		if (errorMessages != 0) { errorMessages->Release(); }

		return 0;
	}

	if (errorMessages != 0) { errorMessages->Release(); }

	return compiledShader;
}

bool GpuProgram::createShaderByType(const Device& device, ShaderType shaderType, ID3DBlob* compiledShader)
{
	HRESULT hr = S_OK;
	switch (shaderType)
	{
		case VERTEX_SHADER:
		{
			hr = device.device->CreateVertexShader(compiledShader->GetBufferPointer(), 
												   compiledShader->GetBufferSize(), 
												   0, &m_vertexShader);
			break;
		}

		case HULL_SHADER:
		{
			hr = device.device->CreateHullShader(compiledShader->GetBufferPointer(),
												 compiledShader->GetBufferSize(),
												 0, &m_hullShader);
			break;
		}

		case DOMAIN_SHADER:
		{
			hr = device.device->CreateDomainShader(compiledShader->GetBufferPointer(),
												   compiledShader->GetBufferSize(),
												   0, &m_domainShader);
			break;
		}

		case GEOMETRY_SHADER:
		{
			hr = device.device->CreateGeometryShader(compiledShader->GetBufferPointer(),
													 compiledShader->GetBufferSize(),
													 0, &m_geometryShader);
			break;
		}

		case PIXEL_SHADER:
		{
			hr = device.device->CreatePixelShader(compiledShader->GetBufferPointer(),
												  compiledShader->GetBufferSize(),
												  0, &m_pixelShader);
			break;
		}

		case COMPUTE_SHADER:
		{
			hr = device.device->CreateComputeShader(compiledShader->GetBufferPointer(),
													compiledShader->GetBufferSize(),
													0, &m_computeShader);
			break;
		}
	}

	if (hr != S_OK)
	{
		utils::Logger::toLogWithFormat("Error: could not create %s.\n", toString(shaderType));
		return false;
	}

	return true;
}

bool GpuProgram::reflectShaders(const Device& device, bool autoInputLayout)
{
	HRESULT hr = S_OK;
	bool reflectionFailed = false;
	for (size_t i = 0; i < m_data.size(); i++)
	{
		if (m_data[i].compiledShader != 0)
		{
			ID3D11ShaderReflection* reflector = 0;
			hr = D3DReflect(m_data[i].compiledShader->GetBufferPointer(),
							m_data[i].compiledShader->GetBufferSize(),
							IID_ID3D11ShaderReflection, reinterpret_cast<void**>(&reflector));
			if (hr != S_OK)
			{
				reflectionFailed = true;
				reflector->Release();
				break;
			}

			// shader desc
			D3D11_SHADER_DESC desc;
			hr = reflector->GetDesc(&desc);
			if (hr != S_OK)
			{
				reflectionFailed = true;
				reflector->Release();
				break;
			}

			// create input layout
			if (autoInputLayout && i == VERTEX_SHADER)
			{
				m_inputLayout.inputLayoutInfo.resize(desc.InputParameters);
				unsigned int byteOffset = 0;
				for (UINT j = 0; j < desc.InputParameters; j++)
				{
					D3D11_SIGNATURE_PARAMETER_DESC input_desc;
					hr = reflector->GetInputParameterDesc(j, &input_desc);
					if (hr != S_OK) { reflectionFailed = true; break; }

					m_inputLayout.inputLayoutInfo[j].SemanticName = stringInPool(input_desc.SemanticName);
					m_inputLayout.inputLayoutInfo[j].SemanticIndex = input_desc.SemanticIndex;
					unsigned int sz = sizeByMask<float>(input_desc.Mask);
					if (sz == 0) { reflectionFailed = true; break; }
					DXGI_FORMAT format = getComponentFormat(input_desc.ComponentType, input_desc.Mask);
					if (format == DXGI_FORMAT_UNKNOWN) { reflectionFailed = true; break; }
					m_inputLayout.inputLayoutInfo[j].Format = format;
					m_inputLayout.inputLayoutInfo[j].InputSlot = 0;
					m_inputLayout.inputLayoutInfo[j].AlignedByteOffset = byteOffset;
					byteOffset += sz;
					m_inputLayout.inputLayoutInfo[j].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
					m_inputLayout.inputLayoutInfo[j].InstanceDataStepRate = 0;
				}
				if (reflectionFailed)
				{
					m_inputLayout.clear();
					reflector->Release();
					break;
				}

				hr = device.device->CreateInputLayout(m_inputLayout.inputLayoutInfo.data(), m_inputLayout.inputLayoutInfo.size(),
													  m_data[i].compiledShader->GetBufferPointer(),
													  m_data[i].compiledShader->GetBufferSize(),
													  &m_inputLayout.inputLayout);
				if (hr != S_OK)
				{
					utils::Logger::toLog("Error: could not create an input layout.\n");
					m_inputLayout.clear();
					reflector->Release();
					break;
				}
			}

			// constant buffers
			m_data[i].m_resources.reserve(32);
			for (UINT j = 0; j < desc.ConstantBuffers; j++)
			{
				std::shared_ptr<ShaderResourceData> cbdata(new ShaderResourceData);

				// buffer info
				ID3D11ShaderReflectionConstantBuffer* constBuffer = reflector->GetConstantBufferByIndex(j);
				D3D11_SHADER_BUFFER_DESC bufferDesc;
				constBuffer->GetDesc(&bufferDesc);
				cbdata->name = bufferDesc.Name;
				cbdata->sizeInBytes = bufferDesc.Size;
				
				// binding info
				D3D11_SHADER_INPUT_BIND_DESC bindDesc;
				hr = reflector->GetResourceBindingDescByName(bufferDesc.Name, &bindDesc);
				if (hr != S_OK)
				{
					reflectionFailed = true;
					reflector->Release();
					break;
				}
				cbdata->bindingPoint = bindDesc.BindPoint;
				cbdata->bindingCount = bindDesc.BindCount;

				m_data[i].m_resources.push_back(cbdata);
			}
			if (reflectionFailed) break;

			// textures and samplers
			for (UINT j = 0; j < desc.BoundResources; j++)
			{
				D3D11_SHADER_INPUT_BIND_DESC resDesc;
				hr = reflector->GetResourceBindingDesc(j, &resDesc);
				if (hr != S_OK)
				{
					reflectionFailed = true;
					reflector->Release();
					break;
				}

				if (resDesc.Type == D3D_SIT_TEXTURE || resDesc.Type == D3D_SIT_SAMPLER ||
					resDesc.Type == D3D_SIT_UAV_RWTYPED || resDesc.Type == D3D_SIT_UAV_RWSTRUCTURED ||
					resDesc.Type == D3D_SIT_UAV_RWBYTEADDRESS || resDesc.Type == D3D_SIT_UAV_APPEND_STRUCTURED || 
					resDesc.Type == D3D_SIT_UAV_CONSUME_STRUCTURED || resDesc.Type == D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER)
				{
					std::shared_ptr<ShaderResourceData> cbdata(new ShaderResourceData);
					cbdata->name = resDesc.Name;
					cbdata->sizeInBytes = 0;
					cbdata->bindingPoint = resDesc.BindPoint;
					cbdata->bindingCount = resDesc.BindCount;

					m_data[i].m_resources.push_back(cbdata);
				}
			}
			if (reflectionFailed) break;

			// release all
			reflector->Release();
		}
	}

	return !reflectionFailed;
}

const char* GpuProgram::stringInPool(const char* str)
{
	std::string s = str;
	m_inputLayoutStringsPool.insert(m_inputLayoutStringsPool.begin(), s);
	return m_inputLayoutStringsPool.begin()->c_str();
}

int GpuProgram::bindInputLayoutInfo(const std::vector<D3D11_INPUT_ELEMENT_DESC>& info)
{
	const Device& device = Application::instance()->getDevice();

	if (m_vertexShader == 0)
	{
		utils::Logger::toLog("Error: could not bind an input layout. The reason: there is no a vertex shader.\n");
		return -1;
	}

	auto it = std::find_if(m_inputLayoutInfoBase.begin(), 
						   m_inputLayoutInfoBase.end(), 
						   [&](const InputLayoutData& layoutData)->bool
	{
		return compareInputLayoutInfos(layoutData.inputLayoutInfo, info);
	});

	if (it != m_inputLayoutInfoBase.end())
	{
		return (int)(it - m_inputLayoutInfoBase.begin());
	}

	InputLayoutData dat;
	dat.inputLayoutInfo = info;
	m_inputLayoutInfoBase.push_back(dat);
	int newindex = ((int)m_inputLayoutInfoBase.size()) - 1;

	HRESULT hr = device.device->CreateInputLayout(m_inputLayoutInfoBase[newindex].inputLayoutInfo.data(),
												  m_inputLayoutInfoBase[newindex].inputLayoutInfo.size(),
												  m_data[VERTEX_SHADER].compiledShader->GetBufferPointer(),
												  m_data[VERTEX_SHADER].compiledShader->GetBufferSize(),
												  &m_inputLayoutInfoBase[newindex].inputLayout);
	if (hr != S_OK)
	{
		utils::Logger::toLog("Error: could not create and bind an input layout.\n");
		auto it = m_inputLayoutInfoBase.end();
		it--;
		m_inputLayoutInfoBase.erase(it);
		return -1;
	}
	
	return newindex;
}

bool GpuProgram::compareInputLayoutInfos(const std::vector<D3D11_INPUT_ELEMENT_DESC>& info1, const std::vector<D3D11_INPUT_ELEMENT_DESC>& info2)
{
	if (info1.size() != info2.size()) return false;
	for (size_t i = 0; i < info1.size(); i++)
	{
		if (strcmp(info1[i].SemanticName, info2[i].SemanticName) != 0) return false;
		if (info1[i].SemanticIndex != info2[i].SemanticIndex) return false;
		if (info1[i].Format != info2[i].Format) return false;
		if (info1[i].InputSlot != info2[i].InputSlot) return false;
		if (info1[i].AlignedByteOffset != info2[i].AlignedByteOffset) return false;
		if (info1[i].InputSlotClass != info2[i].InputSlotClass) return false;
		if (info1[i].InstanceDataStepRate != info2[i].InstanceDataStepRate) return false;
	}
	return true;
}

bool GpuProgram::use()
{
	if (!isValid()) return false;

	const Device& device = Application::instance()->getDevice();

	// set shader
	device.context->VSSetShader(m_vertexShader, 0, 0);
	device.context->HSSetShader(m_hullShader, 0, 0);
	device.context->DSSetShader(m_domainShader, 0, 0);
	device.context->GSSetShader(m_geometryShader, 0, 0);
	device.context->PSSetShader(m_pixelShader, 0, 0);
	device.context->CSSetShader(m_computeShader, 0, 0);

	// auto input layout
	if (m_inputLayout.inputLayout != 0)
	{
		device.context->IASetInputLayout(m_inputLayout.inputLayout);
	}

	// set using GPU-program
	Application::instance()->setUsingGpuProgram(std::static_pointer_cast<GpuProgram>(shared_from_this()));

	return true;
}

void GpuProgram::applyInputLayout(int inputLayoutIndex)
{
	if (!isValid()) return;

	const Device& device = Application::instance()->getDevice();

	if (inputLayoutIndex >= 0 && inputLayoutIndex < (int)m_inputLayoutInfoBase.size() && 
		m_inputLayoutInfoBase[inputLayoutIndex].inputLayout != 0)
	{
		device.context->IASetInputLayout(m_inputLayoutInfoBase[inputLayoutIndex].inputLayout);
	}
}

void GpuProgram::bindUniformByIndex(int index, const std::string& name)
{
	bool found = false;
	for (size_t i = 0; i < m_data.size(); i++)
	{
		std::for_each(m_data[i].m_resources.begin(), m_data[i].m_resources.end(), [&](const std::shared_ptr<ShaderResourceData>& cb)
		{
			if (cb->name == name)
			{
				m_uniforms[i][index] = cb;
				found = true;
			}
		});
	}
	if (!found)
	{
		utils::Logger::toLogWithFormat("Error: Uniform '%s' has not been found to bind.\n", name.c_str());
		return;
	}

	// check size
	unsigned int sz = 0;
	bool failed = false;
	for (size_t i = 0; i < m_data.size(); i++)
	{
		if (!m_uniforms[i][index].expired())
		{
			auto ptr = m_uniforms[i][index].lock();
			if (sz == 0) sz = ptr->sizeInBytes;
			else if (sz != ptr->sizeInBytes)
			{
				utils::Logger::toLogWithFormat("Error: could not bind an uniform '%s'. The reason: the uniform has different size in shaders.\n", name.c_str());
				failed = true;
				break;
			}
		}
	}
	if (failed)
	{
		for (size_t i = 0; i < m_data.size(); i++) m_uniforms[i][index].reset();
	}
}

void GpuProgram::setUniformByIndex(int index, std::shared_ptr<UniformBuffer> buffer)
{
	if (buffer.get() == 0) return;
	const Device& device = Application::instance()->getDevice();

	// check size
	if (buffer->checkSizeOnSet())
	{
		for (size_t i = 0; i < m_data.size(); i++)
		{
			if (!m_uniforms[i][index].expired())
			{
				auto ptr = m_uniforms[i][index].lock();
				if (ptr->sizeInBytes != buffer->getElementByteSize())
				{
					utils::Logger::toLogWithFormat("Error: could not set an uniform. The reason: the uniform buffer's size does not match the expected size.\n");
					return;
				}
			}
		}
	}

	ID3D11Buffer* buf = buffer->getBuffer();
	ID3D11ShaderResourceView* view = buffer->getView().asShaderView();
	if (m_vertexShader != 0 && !m_uniforms[VERTEX_SHADER][index].expired())
	{
		auto ptr = m_uniforms[VERTEX_SHADER][index].lock();
		if (buffer->isStructured()) device.context->VSSetShaderResources(ptr->bindingPoint, 1, &view);
		else device.context->VSSetConstantBuffers(ptr->bindingPoint, 1, (ID3D11Buffer * const *)&buf);
	}
	if (m_hullShader != 0 && !m_uniforms[HULL_SHADER][index].expired())
	{
		auto ptr = m_uniforms[HULL_SHADER][index].lock();
		if (buffer->isStructured()) device.context->HSSetShaderResources(ptr->bindingPoint, 1, &view);
		else device.context->HSSetConstantBuffers(ptr->bindingPoint, 1, (ID3D11Buffer * const *)&buf);
	}
	if (m_domainShader != 0 && !m_uniforms[DOMAIN_SHADER][index].expired())
	{
		auto ptr = m_uniforms[DOMAIN_SHADER][index].lock();
		if (buffer->isStructured()) device.context->DSSetShaderResources(ptr->bindingPoint, 1, &view);
		else device.context->DSSetConstantBuffers(ptr->bindingPoint, 1, (ID3D11Buffer * const *)&buf);
	}
	if (m_geometryShader != 0 && !m_uniforms[GEOMETRY_SHADER][index].expired())
	{
		auto ptr = m_uniforms[GEOMETRY_SHADER][index].lock();
		if (buffer->isStructured()) device.context->GSSetShaderResources(ptr->bindingPoint, 1, &view);
		else device.context->GSSetConstantBuffers(ptr->bindingPoint, 1, (ID3D11Buffer * const *)&buf);
	}
	if (m_pixelShader != 0 && !m_uniforms[PIXEL_SHADER][index].expired())
	{
		auto ptr = m_uniforms[PIXEL_SHADER][index].lock();
		if (buffer->isStructured()) device.context->PSSetShaderResources(ptr->bindingPoint, 1, &view);
		else device.context->PSSetConstantBuffers(ptr->bindingPoint, 1, (ID3D11Buffer * const *)&buf);
	}
	if (m_computeShader != 0 && !m_uniforms[COMPUTE_SHADER][index].expired())
	{
		auto ptr = m_uniforms[COMPUTE_SHADER][index].lock();
		ID3D11UnorderedAccessView* uaview = buffer->getView().asUAView();
		if (buffer->isStructured())
		{
			device.context->CSSetShaderResources(ptr->bindingPoint, 1, &view);
			if (uaview != 0)
			{
				UINT UAVInitialCount = -1;
				device.context->CSSetUnorderedAccessViews(ptr->bindingPoint, 1, &uaview, &UAVInitialCount);
			}
		}
		else
		{
			device.context->CSSetConstantBuffers(ptr->bindingPoint, 1, (ID3D11Buffer * const *)&buf);
		}
	}
}

void GpuProgram::setUniformByIndex( int index, std::shared_ptr<Texture> texture )
{
	if (texture.get() == 0) return;

	ID3D11ShaderResourceView* view = texture->getView();
	setUniformByIndex(index, view);
}

void GpuProgram::setUniformByIndex(int index, ID3D11ShaderResourceView* view)
{
	const Device& device = Application::instance()->getDevice();
	if (m_vertexShader != 0 && !m_uniforms[VERTEX_SHADER][index].expired())
	{
		auto ptr = m_uniforms[VERTEX_SHADER][index].lock();
		device.context->VSSetShaderResources(ptr->bindingPoint, 1, &view);
	}
	if (m_hullShader != 0 && !m_uniforms[HULL_SHADER][index].expired())
	{
		auto ptr = m_uniforms[HULL_SHADER][index].lock();
		device.context->HSSetShaderResources(ptr->bindingPoint, 1, &view);
	}
	if (m_domainShader != 0 && !m_uniforms[DOMAIN_SHADER][index].expired())
	{
		auto ptr = m_uniforms[DOMAIN_SHADER][index].lock();
		device.context->DSSetShaderResources(ptr->bindingPoint, 1, &view);
	}
	if (m_geometryShader != 0 && !m_uniforms[GEOMETRY_SHADER][index].expired())
	{
		auto ptr = m_uniforms[GEOMETRY_SHADER][index].lock();
		device.context->GSSetShaderResources(ptr->bindingPoint, 1, &view);
	}
	if (m_pixelShader != 0 && !m_uniforms[PIXEL_SHADER][index].expired())
	{
		auto ptr = m_uniforms[PIXEL_SHADER][index].lock();
		device.context->PSSetShaderResources(ptr->bindingPoint, 1, &view);

		ID3D11SamplerState* samplerState = 0;
		device.context->PSSetSamplers(ptr->bindingPoint, 1, &samplerState);
	}
	if (m_computeShader != 0 && !m_uniforms[COMPUTE_SHADER][index].expired())
	{
		auto ptr = m_uniforms[COMPUTE_SHADER][index].lock();
		device.context->CSSetShaderResources(ptr->bindingPoint, 1, &view);
	}
}

void GpuProgram::setUniformByIndex(int index, std::shared_ptr<Sampler> sampler)
{
	if (sampler.get() == 0) return;
	const Device& device = Application::instance()->getDevice();

	ID3D11SamplerState* samplerState = sampler->getSampler();
	if (m_vertexShader != 0 && !m_uniforms[VERTEX_SHADER][index].expired())
	{
		auto ptr = m_uniforms[VERTEX_SHADER][index].lock();
		device.context->VSSetSamplers(ptr->bindingPoint, 1, &samplerState);
	}
	if (m_hullShader != 0 && !m_uniforms[HULL_SHADER][index].expired())
	{
		auto ptr = m_uniforms[HULL_SHADER][index].lock();
		device.context->HSSetSamplers(ptr->bindingPoint, 1, &samplerState);
	}
	if (m_domainShader != 0 && !m_uniforms[DOMAIN_SHADER][index].expired())
	{
		auto ptr = m_uniforms[DOMAIN_SHADER][index].lock();
		device.context->DSSetSamplers(ptr->bindingPoint, 1, &samplerState);
	}
	if (m_geometryShader != 0 && !m_uniforms[GEOMETRY_SHADER][index].expired())
	{
		auto ptr = m_uniforms[GEOMETRY_SHADER][index].lock();
		device.context->GSSetSamplers(ptr->bindingPoint, 1, &samplerState);
	}
	if (m_pixelShader != 0 && !m_uniforms[PIXEL_SHADER][index].expired())
	{
		auto ptr = m_uniforms[PIXEL_SHADER][index].lock();
		device.context->PSSetSamplers(ptr->bindingPoint, 1, &samplerState);
	}
	if (m_computeShader != 0 && !m_uniforms[COMPUTE_SHADER][index].expired())
	{
		auto ptr = m_uniforms[COMPUTE_SHADER][index].lock();
		device.context->CSSetSamplers(ptr->bindingPoint, 1, &samplerState);
	}
}
void GpuProgram::setUniformByIndex(int index, std::shared_ptr<RenderTarget> renderTarget, unsigned int rtIndex)
{
	if (renderTarget.get() == 0) return;

	ID3D11ShaderResourceView* view = renderTarget->getView(rtIndex).asShaderView();
	setUniformByIndex(index, view);
}

int GpuProgram::generateId()
{
	static int id = -1;
	id++;
	return id;
}

GpuProgram::InputLayoutData::~InputLayoutData()
{
	clear();
}

void GpuProgram::InputLayoutData::clear()
{
	if (inputLayout) { inputLayout->Release(); inputLayout = 0; }
	inputLayoutInfo.clear();
}

}