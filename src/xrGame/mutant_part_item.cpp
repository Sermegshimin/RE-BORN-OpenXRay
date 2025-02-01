#include "StdAfx.h"
#include "mutant_part_item.h"

//Eatable Mutant Parts

#define MAX_FRESHNESS 1.f
#define MIN_FRESHNESS 0.f

CEatableMutantPartItem::CEatableMutantPartItem() 
{ 
    m_iQuality = 0; 
    m_fFreshness = MAX_FRESHNESS;
}

CEatableMutantPartItem::~CEatableMutantPartItem() {}

void CEatableMutantPartItem::load(IReader& packet)
{
    inherited::load(packet);

    m_iQuality = packet.r_u8();
    m_fFreshness = packet.r_u8();
}

void CEatableMutantPartItem::save(NET_Packet& packet)
{
    inherited::save(packet);

    packet.w_u8(m_iQuality);
    packet.w_float(m_fFreshness);
}

void CEatableMutantPartItem::UpdateFreshness(float value)
{
    m_fFreshness += value;
    if (m_fFreshness >= MAX_FRESHNESS)
        m_fFreshness = MAX_FRESHNESS;
    if (m_fFreshness <= MIN_FRESHNESS)
        m_fFreshness = MIN_FRESHNESS;
}

//Usual Mutant Parts

CGeneralMutantPartItem::CGeneralMutantPartItem() { m_iQuality = 0; }

CGeneralMutantPartItem::~CGeneralMutantPartItem() {}

void CGeneralMutantPartItem::load(IReader& packet)
{
    inherited::CInventoryItem::load(packet);

    m_iQuality = packet.r_u8();
}

void CGeneralMutantPartItem::save(NET_Packet& packet)
{
    inherited::save(packet);

    packet.w_u8(m_iQuality);
}

