#pragma once


#include "eatable_item_object.h"

class CEatableMutantPartItem : public CEatableItemObject
{
private:
    typedef CEatableItemObject inherited;

protected:
    u8 m_iQuality;
    float m_fFreshness;

public:
    CEatableMutantPartItem();
    virtual ~CEatableMutantPartItem();
    virtual CEatableMutantPartItem* cast_eatable_mutant_part_item() { return this; }
    void load(IReader& packet) override;
    void save(NET_Packet& packet) override;
    void SetQuality(u8 quality) { m_iQuality = quality; };
    u8 GetQuality() const { return m_iQuality; };
    void UpdateFreshness(float value);
    float GetFreshness() const { return m_fFreshness; };
};

class CGeneralMutantPartItem : public CInventoryItemObject
{
private:
    typedef CInventoryItemObject inherited;

protected:
    u8 m_iQuality;

public:
    CGeneralMutantPartItem();
    virtual ~CGeneralMutantPartItem();
    virtual CGeneralMutantPartItem* cast_general_mutant_part_item() { return this; }
    void load(IReader& packet) override;
    void save(NET_Packet& packet) override;
    void SetQuality(u8 quality) { m_iQuality = quality; };
    u8 GetQuality() const { return m_iQuality; };
};
