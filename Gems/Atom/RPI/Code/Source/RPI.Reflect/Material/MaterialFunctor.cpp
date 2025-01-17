/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Atom/RPI.Reflect/Material/MaterialFunctor.h>
#include <Atom/RPI.Reflect/Material/MaterialPropertiesLayout.h>
#include <Atom/RPI.Reflect/Material/MaterialPropertyCollection.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <Atom/RPI.Reflect/Material/ShaderCollection.h>
#include <AzCore/Math/Vector2.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/Math/Vector4.h>
#include <AzCore/Math/Color.h>

namespace AZ
{
    namespace RPI
    {
        void MaterialFunctor::Reflect(ReflectContext* context)
        {
            if (auto* serializeContext = azrtti_cast<SerializeContext*>(context))
            {
                serializeContext->Class<MaterialFunctor>()
                    ->Version(2)
                    ->Field("materialPropertyDependencies", &MaterialFunctor::m_materialPropertyDependencies)
                    ;
            }
        }

        MaterialFunctor::RuntimeContext::RuntimeContext(
            const MaterialPropertyCollection& materialProperties,
            MaterialPipelineShaderCollections* shaderCollections,
            ShaderResourceGroup* shaderResourceGroup,
            const MaterialPropertyFlags* materialPropertyDependencies,
            MaterialPropertyPsoHandling psoHandling
        )
            : m_materialProperties(materialProperties)
            , m_allShaderCollections(shaderCollections)
            , m_shaderResourceGroup(shaderResourceGroup)
            , m_materialPropertyDependencies(materialPropertyDependencies)
            , m_psoHandling(psoHandling)
        {
            static ShaderCollection EmptyShaderCollection;

            auto iter = m_allShaderCollections->find(MaterialPipelineNameCommon);
            if (iter != m_allShaderCollections->end())
            {
                m_commonShaderCollection = &iter->second;
            }
            else
            {
                m_commonShaderCollection = &EmptyShaderCollection;
            }
        }

        template<typename ValueType>
        bool MaterialFunctor::RuntimeContext::SetShaderOptionValueHelper(const Name& name, const ValueType& value)
        {
            bool didSetOne = false;

            for (auto& shaderCollectionPair : *m_allShaderCollections)
            {
                for (ShaderCollection::Item& shaderItem : shaderCollectionPair.second)
                {
                    ShaderOptionGroup* shaderOptionGroup = shaderItem.GetShaderOptions();
                    const ShaderOptionGroupLayout* layout = shaderOptionGroup->GetShaderOptionLayout();
                    ShaderOptionIndex optionIndex = layout->FindShaderOptionIndex(name);

                    if (!optionIndex.IsValid())
                    {
                        continue;
                    }

                    if (!shaderItem.MaterialOwnsShaderOption(optionIndex))
                    {
                        AZ_Error("MaterialFunctor", false, "Shader option '%s' is not owned by this material.", name.GetCStr());
                        AZ_Assert(!didSetOne, "The material build pipeline should have ensured that MaterialOwnsShaderOption is consistent across all shaders.");
                        return false;
                    }

                    if (shaderOptionGroup->SetValue(optionIndex, value))
                    {
                        didSetOne = true;
                    }
                }
            }

            if (!didSetOne)
            {
                AZ_Error("MaterialFunctor", false, "Shader option '%s' not found.", name.GetCStr());
            }

            return didSetOne;
        }

        bool MaterialFunctor::RuntimeContext::SetShaderOptionValue(const Name& optionName, ShaderOptionValue value)
        {
            return SetShaderOptionValueHelper(optionName, value);
        }

        bool MaterialFunctor::RuntimeContext::SetShaderOptionValue(const Name& optionName, const Name& value)
        {
            return SetShaderOptionValueHelper(optionName, value);
        }

        ShaderResourceGroup* MaterialFunctor::RuntimeContext::GetShaderResourceGroup()
        {
            return m_shaderResourceGroup;
        }

        AZStd::size_t MaterialFunctor::RuntimeContext::GetShaderCount() const
        {
            return m_commonShaderCollection->size();
        }

        void MaterialFunctor::RuntimeContext::SetShaderEnabled(AZStd::size_t shaderIndex, bool enabled)
        {
            (*m_commonShaderCollection)[shaderIndex].SetEnabled(enabled);
        }

        void MaterialFunctor::RuntimeContext::SetShaderEnabled(const AZ::Name& shaderTag, bool enabled)
        {
            (*m_commonShaderCollection)[shaderTag].SetEnabled(enabled);
        }

        void MaterialFunctor::RuntimeContext::SetShaderDrawListTagOverride(AZStd::size_t shaderIndex, const Name& drawListTagName)
        {
            (*m_commonShaderCollection)[shaderIndex].SetDrawListTagOverride(drawListTagName);
        }

        void MaterialFunctor::RuntimeContext::SetShaderDrawListTagOverride(const AZ::Name& shaderTag, const Name& drawListTagName)
        {
            (*m_commonShaderCollection)[shaderTag].SetDrawListTagOverride(drawListTagName);
        }

        void MaterialFunctor::RuntimeContext::ApplyShaderRenderStateOverlay(AZStd::size_t shaderIndex, const RHI::RenderStates& renderStatesOverlay)
        {
            RHI::MergeStateInto(renderStatesOverlay, *((*m_commonShaderCollection)[shaderIndex].GetRenderStatesOverlay()));
        }

        void MaterialFunctor::RuntimeContext::ApplyShaderRenderStateOverlay(const AZ::Name& shaderTag, const RHI::RenderStates& renderStatesOverlay)
        {
            RHI::MergeStateInto(renderStatesOverlay, *((*m_commonShaderCollection)[shaderTag].GetRenderStatesOverlay()));
        }

        MaterialFunctor::EditorContext::EditorContext(
            const MaterialPropertyCollection& materialProperties,
            AZStd::unordered_map<Name, MaterialPropertyDynamicMetadata>& propertyMetadata,
            AZStd::unordered_map<Name, MaterialPropertyGroupDynamicMetadata>& propertyGroupMetadata,
            AZStd::unordered_set<Name>& updatedPropertiesOut,
            AZStd::unordered_set<Name>& updatedPropertyGroupsOut,
            const MaterialPropertyFlags* materialPropertyDependencies
        )
            : m_materialProperties(materialProperties)
            , m_propertyMetadata(propertyMetadata)
            , m_propertyGroupMetadata(propertyGroupMetadata)
            , m_updatedPropertiesOut(updatedPropertiesOut)
            , m_updatedPropertyGroupsOut(updatedPropertyGroupsOut)
            , m_materialPropertyDependencies(materialPropertyDependencies)
        {}

        const MaterialPropertyDynamicMetadata* MaterialFunctor::EditorContext::GetMaterialPropertyMetadata(const Name& propertyId) const
        {
            return QueryMaterialPropertyMetadata(propertyId);
        }

        const MaterialPropertyDynamicMetadata* MaterialFunctor::EditorContext::GetMaterialPropertyMetadata(const MaterialPropertyIndex& index) const
        {
            const Name& name = m_materialProperties.GetMaterialPropertiesLayout()->GetPropertyDescriptor(index)->GetName();
            return GetMaterialPropertyMetadata(name);
        }
        
        const MaterialPropertyGroupDynamicMetadata* MaterialFunctor::EditorContext::GetMaterialPropertyGroupMetadata(const Name& propertyId) const
        {
            return QueryMaterialPropertyGroupMetadata(propertyId);
        }
        
        bool MaterialFunctor::EditorContext::SetMaterialPropertyGroupVisibility(const Name& propertyGroupName, MaterialPropertyGroupVisibility visibility)
        {
            MaterialPropertyGroupDynamicMetadata* metadata = QueryMaterialPropertyGroupMetadata(propertyGroupName);
            if (!metadata)
            {
                return false;
            }

            if (metadata->m_visibility != visibility)
            {
                metadata->m_visibility = visibility;
                m_updatedPropertyGroupsOut.insert(propertyGroupName);
            }

            return true;
        }

        bool MaterialFunctor::EditorContext::SetMaterialPropertyVisibility(const Name& propertyId, MaterialPropertyVisibility visibility)
        {
            MaterialPropertyDynamicMetadata* metadata = QueryMaterialPropertyMetadata(propertyId);
            if (!metadata)
            {
                return false;
            }

            if (metadata->m_visibility != visibility)
            {
                metadata->m_visibility = visibility;
                m_updatedPropertiesOut.insert(propertyId);
            }

            return true;
        }

        bool MaterialFunctor::EditorContext::SetMaterialPropertyVisibility(const MaterialPropertyIndex& index, MaterialPropertyVisibility visibility)
        {
            const Name& name = m_materialProperties.GetMaterialPropertiesLayout()->GetPropertyDescriptor(index)->GetName();
            return SetMaterialPropertyVisibility(name, visibility);
        }

        bool MaterialFunctor::EditorContext::SetMaterialPropertyDescription(const Name& propertyId, AZStd::string description)
        {
            MaterialPropertyDynamicMetadata* metadata = QueryMaterialPropertyMetadata(propertyId);
            if (!metadata)
            {
                return false;
            }

            if (metadata->m_description != description)
            {
                metadata->m_description = description;
                m_updatedPropertiesOut.insert(propertyId);
            }

            return true;
        }

        bool MaterialFunctor::EditorContext::SetMaterialPropertyDescription(const MaterialPropertyIndex& index, AZStd::string description)
        {
            const Name& name = m_materialProperties.GetMaterialPropertiesLayout()->GetPropertyDescriptor(index)->GetName();
            return SetMaterialPropertyDescription(name, description);
        }

        bool MaterialFunctor::EditorContext::SetMaterialPropertyMinValue(const Name& propertyId, const MaterialPropertyValue& min)
        {
            MaterialPropertyDynamicMetadata* metadata = QueryMaterialPropertyMetadata(propertyId);
            if (!metadata)
            {
                return false;
            }

            if(metadata->m_propertyRange.m_min != min)
            {
                metadata->m_propertyRange.m_min = min;
                m_updatedPropertiesOut.insert(propertyId);
            }

            return true;
        }

        bool MaterialFunctor::EditorContext::SetMaterialPropertyMinValue(const MaterialPropertyIndex& index, const MaterialPropertyValue& min)
        {
            const Name& name = m_materialProperties.GetMaterialPropertiesLayout()->GetPropertyDescriptor(index)->GetName();
            return SetMaterialPropertyMinValue(name, min);
        }

        bool MaterialFunctor::EditorContext::SetMaterialPropertyMaxValue(const Name& propertyId, const MaterialPropertyValue& max)
        {
            MaterialPropertyDynamicMetadata* metadata = QueryMaterialPropertyMetadata(propertyId);
            if (!metadata)
            {
                return false;
            }

            if (metadata->m_propertyRange.m_max != max)
            {
                metadata->m_propertyRange.m_max = max;
                m_updatedPropertiesOut.insert(propertyId);
            }

            return true;
        }

        bool MaterialFunctor::EditorContext::SetMaterialPropertyMaxValue(const MaterialPropertyIndex& index, const MaterialPropertyValue& max)
        {
            const Name& name = m_materialProperties.GetMaterialPropertiesLayout()->GetPropertyDescriptor(index)->GetName();
            return SetMaterialPropertyMaxValue(name, max);
        }

        bool MaterialFunctor::EditorContext::SetMaterialPropertySoftMinValue(const Name& propertyId, const MaterialPropertyValue& min)
        {
            MaterialPropertyDynamicMetadata* metadata = QueryMaterialPropertyMetadata(propertyId);
            if (!metadata)
            {
                return false;
            }

            if (metadata->m_propertyRange.m_softMin != min)
            {
                metadata->m_propertyRange.m_softMin = min;
                m_updatedPropertiesOut.insert(propertyId);
            }

            return true;
        }

        bool MaterialFunctor::EditorContext::SetMaterialPropertySoftMinValue(const MaterialPropertyIndex& index, const MaterialPropertyValue& min)
        {
            const Name& name = m_materialProperties.GetMaterialPropertiesLayout()->GetPropertyDescriptor(index)->GetName();
            return SetMaterialPropertySoftMinValue(name, min);
        }

        bool MaterialFunctor::EditorContext::SetMaterialPropertySoftMaxValue(const Name& propertyId, const MaterialPropertyValue& max)
        {
            MaterialPropertyDynamicMetadata* metadata = QueryMaterialPropertyMetadata(propertyId);
            if (!metadata)
            {
                return false;
            }

            if (metadata->m_propertyRange.m_softMax != max)
            {
                metadata->m_propertyRange.m_softMax = max;
                m_updatedPropertiesOut.insert(propertyId);
            }

            return true;
        }

        bool MaterialFunctor::EditorContext::SetMaterialPropertySoftMaxValue(const MaterialPropertyIndex& index, const MaterialPropertyValue& max)
        {
            const Name& name = m_materialProperties.GetMaterialPropertiesLayout()->GetPropertyDescriptor(index)->GetName();
            return SetMaterialPropertySoftMaxValue(name, max);
        }

        MaterialPropertyDynamicMetadata* MaterialFunctor::EditorContext::QueryMaterialPropertyMetadata(const Name& propertyId) const
        {
            auto it = m_propertyMetadata.find(propertyId);
            if (it == m_propertyMetadata.end())
            {
                AZ_Error("MaterialFunctor", false, "Couldn't find metadata for material property: %s.", propertyId.GetCStr());
                return nullptr;
            }

            return &it->second;
        }
        
        MaterialPropertyGroupDynamicMetadata* MaterialFunctor::EditorContext::QueryMaterialPropertyGroupMetadata(const Name& propertyGroupId) const
        {
            auto it = m_propertyGroupMetadata.find(propertyGroupId);
            if (it == m_propertyGroupMetadata.end())
            {
                AZ_Error("MaterialFunctor", false, "Couldn't find metadata for material property group: %s.", propertyGroupId.GetCStr());
                return nullptr;
            }

            return &it->second;
        }

        template<typename Type>
        const Type& MaterialFunctor::RuntimeContext::GetMaterialPropertyValue(const MaterialPropertyIndex& index) const
        {
            return GetMaterialPropertyValue(index).GetValue<Type>();
        }

        // explicit template instantiation
        template const bool&     MaterialFunctor::RuntimeContext::GetMaterialPropertyValue<bool>     (const Name& propertyId) const;
        template const int32_t&  MaterialFunctor::RuntimeContext::GetMaterialPropertyValue<int32_t>  (const Name& propertyId) const;
        template const uint32_t& MaterialFunctor::RuntimeContext::GetMaterialPropertyValue<uint32_t> (const Name& propertyId) const;
        template const float&    MaterialFunctor::RuntimeContext::GetMaterialPropertyValue<float>    (const Name& propertyId) const;
        template const Vector2&  MaterialFunctor::RuntimeContext::GetMaterialPropertyValue<Vector2>  (const Name& propertyId) const;
        template const Vector3&  MaterialFunctor::RuntimeContext::GetMaterialPropertyValue<Vector3>  (const Name& propertyId) const;
        template const Vector4&  MaterialFunctor::RuntimeContext::GetMaterialPropertyValue<Vector4>  (const Name& propertyId) const;
        template const Color&    MaterialFunctor::RuntimeContext::GetMaterialPropertyValue<Color>    (const Name& propertyId) const;
        template const Data::Instance<Image>& MaterialFunctor::RuntimeContext::GetMaterialPropertyValue<Data::Instance<Image>>(const Name& propertyId) const;

        template<typename Type>
        const Type& MaterialFunctor::RuntimeContext::GetMaterialPropertyValue(const Name& propertyId) const
        {
            return GetMaterialPropertyValue(propertyId).GetValue<Type>();
        }

        // explicit template instantiation
        template const bool&     MaterialFunctor::RuntimeContext::GetMaterialPropertyValue<bool>     (const MaterialPropertyIndex& index) const;
        template const int32_t&  MaterialFunctor::RuntimeContext::GetMaterialPropertyValue<int32_t>  (const MaterialPropertyIndex& index) const;
        template const uint32_t& MaterialFunctor::RuntimeContext::GetMaterialPropertyValue<uint32_t> (const MaterialPropertyIndex& index) const;
        template const float&    MaterialFunctor::RuntimeContext::GetMaterialPropertyValue<float>    (const MaterialPropertyIndex& index) const;
        template const Vector2&  MaterialFunctor::RuntimeContext::GetMaterialPropertyValue<Vector2>  (const MaterialPropertyIndex& index) const;
        template const Vector3&  MaterialFunctor::RuntimeContext::GetMaterialPropertyValue<Vector3>  (const MaterialPropertyIndex& index) const;
        template const Vector4&  MaterialFunctor::RuntimeContext::GetMaterialPropertyValue<Vector4>  (const MaterialPropertyIndex& index) const;
        template const Color&    MaterialFunctor::RuntimeContext::GetMaterialPropertyValue<Color>    (const MaterialPropertyIndex& index) const;
        template const Data::Instance<Image>& MaterialFunctor::RuntimeContext::GetMaterialPropertyValue<Data::Instance<Image>> (const MaterialPropertyIndex& index) const;

        template<typename Type>
        const Type& MaterialFunctor::EditorContext::GetMaterialPropertyValue(const MaterialPropertyIndex& index) const
        {
            return GetMaterialPropertyValue(index).GetValue<Type>();
        }

        // explicit template instantiation
        template const bool&     MaterialFunctor::EditorContext::GetMaterialPropertyValue<bool>     (const Name& propertyId) const;
        template const int32_t&  MaterialFunctor::EditorContext::GetMaterialPropertyValue<int32_t>  (const Name& propertyId) const;
        template const uint32_t& MaterialFunctor::EditorContext::GetMaterialPropertyValue<uint32_t> (const Name& propertyId) const;
        template const float&    MaterialFunctor::EditorContext::GetMaterialPropertyValue<float>    (const Name& propertyId) const;
        template const Vector2&  MaterialFunctor::EditorContext::GetMaterialPropertyValue<Vector2>  (const Name& propertyId) const;
        template const Vector3&  MaterialFunctor::EditorContext::GetMaterialPropertyValue<Vector3>  (const Name& propertyId) const;
        template const Vector4&  MaterialFunctor::EditorContext::GetMaterialPropertyValue<Vector4>  (const Name& propertyId) const;
        template const Color&    MaterialFunctor::EditorContext::GetMaterialPropertyValue<Color>    (const Name& propertyId) const;

        template<typename Type>
        const Type& MaterialFunctor::EditorContext::GetMaterialPropertyValue(const Name& propertyId) const
        {
            return GetMaterialPropertyValue(propertyId).GetValue<Type>();
        }

        // explicit template instantiation
        template const bool&     MaterialFunctor::EditorContext::GetMaterialPropertyValue<bool>     (const MaterialPropertyIndex& index) const;
        template const int32_t&  MaterialFunctor::EditorContext::GetMaterialPropertyValue<int32_t>  (const MaterialPropertyIndex& index) const;
        template const uint32_t& MaterialFunctor::EditorContext::GetMaterialPropertyValue<uint32_t> (const MaterialPropertyIndex& index) const;
        template const float&    MaterialFunctor::EditorContext::GetMaterialPropertyValue<float>    (const MaterialPropertyIndex& index) const;
        template const Vector2&  MaterialFunctor::EditorContext::GetMaterialPropertyValue<Vector2>  (const MaterialPropertyIndex& index) const;
        template const Vector3&  MaterialFunctor::EditorContext::GetMaterialPropertyValue<Vector3>  (const MaterialPropertyIndex& index) const;
        template const Vector4&  MaterialFunctor::EditorContext::GetMaterialPropertyValue<Vector4>  (const MaterialPropertyIndex& index) const;
        template const Color&    MaterialFunctor::EditorContext::GetMaterialPropertyValue<Color>    (const MaterialPropertyIndex& index) const;
        template const Data::Instance<Image>& MaterialFunctor::EditorContext::GetMaterialPropertyValue<Data::Instance<Image>> (const MaterialPropertyIndex& index) const;

        void CheckPropertyAccess([[maybe_unused]] const MaterialPropertyIndex& index, [[maybe_unused]] const MaterialPropertyFlags& materialPropertyDependencies, [[maybe_unused]] const MaterialPropertiesLayout& materialPropertiesLayout)
        {
#if defined(AZ_ENABLE_TRACING)
            if (!materialPropertyDependencies.test(index.GetIndex()))
            {
                const MaterialPropertyDescriptor* propertyDescriptor = materialPropertiesLayout.GetPropertyDescriptor(index);
                AZ_Error("MaterialFunctor", false, "Material functor accessing an unregistered material property '%s'.",
                    propertyDescriptor ? propertyDescriptor->GetName().GetCStr() : "<unknown>");
            }
#endif
        }

        const MaterialPropertiesLayout* MaterialFunctor::RuntimeContext::GetMaterialPropertiesLayout() const
        {
            return m_materialProperties.GetMaterialPropertiesLayout().get();
        }

        const MaterialPropertyValue& MaterialFunctor::RuntimeContext::GetMaterialPropertyValue(const MaterialPropertyIndex& index) const
        {
            CheckPropertyAccess(index, *m_materialPropertyDependencies, *GetMaterialPropertiesLayout());

            return m_materialProperties.GetPropertyValue(index);
        }

        const MaterialPropertyValue& MaterialFunctor::RuntimeContext::GetMaterialPropertyValue(const Name& propertyId) const
        {
            MaterialPropertyIndex index = GetMaterialPropertiesLayout()->FindPropertyIndex(propertyId);
            return GetMaterialPropertyValue(index);
        }

        const MaterialPropertyValue& MaterialFunctor::EditorContext::GetMaterialPropertyValue(const MaterialPropertyIndex& index) const
        {
            CheckPropertyAccess(index, *m_materialPropertyDependencies, *GetMaterialPropertiesLayout());

            return m_materialProperties.GetPropertyValue(index);
        }

        const MaterialPropertyValue& MaterialFunctor::EditorContext::GetMaterialPropertyValue(const Name& propertyId) const
        {
            MaterialPropertyIndex index = GetMaterialPropertiesLayout()->FindPropertyIndex(propertyId);
            return GetMaterialPropertyValue(index);
        }

        const MaterialPropertiesLayout* MaterialFunctor::EditorContext::GetMaterialPropertiesLayout() const
        {
            return m_materialProperties.GetMaterialPropertiesLayout().get();
        }

        bool MaterialFunctor::NeedsProcess(const MaterialPropertyFlags& propertyDirtyFlags)
        {
            return (m_materialPropertyDependencies & propertyDirtyFlags).any();
        }

        const MaterialPropertyFlags& MaterialFunctor::GetMaterialPropertyDependencies() const
        {
            return m_materialPropertyDependencies;
        }
    } // namespace RPI
} // namespace AZ
