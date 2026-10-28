#include "../map_skill_template.hpp"
#include <cassert>
#include <iostream>
using namespace fourstory::server::map_data;
int main() {
 static_assert(sizeof(DWORD)==4 && sizeof(WORD)==2 && sizeof(__time64_t)==8);
 CTSkillTemp skill;assert(skill.m_vData.empty() && skill.m_mapTSkillPoint.empty());
 skill.m_f1stRateX=2;skill.m_bStartLevel=2;skill.m_bNextLevel=1;skill.m_dwAggro=100;skill.m_fPrice=0.5f;
 auto* data=new TSKILLDATA();data->m_bType=SDT_ABILITY;data->m_bAttr=SATT_PHYSIC;data->m_bExec=MTYPE_DAMAGE;
 data->m_wValue=10;data->m_wValueInc=3;skill.m_vData.push_back(data);
 data->m_bCalc=0;assert(skill.GetValue(data,3)==10);
 data->m_bCalc=1;assert(skill.GetValue(data,3)==16);
 data->m_bCalc=3;assert(skill.GetValue(data,3)==4);
 data->m_bCalc=2;data->m_wValue=100;assert(skill.GetValue(data,2)==8);
 data->m_bCalc=0;data->m_wValue=5;
 data->m_bInc=SVI_INCREASE;assert(skill.Calculate(1,0,100)==5);
 data->m_bInc=SVI_DECREASE;assert(skill.Calculate(1,0,100)==-5);
 data->m_bInc=SVI_MULTIPLY;assert(skill.Calculate(1,0,100)==400);
 data->m_bInc=SVI_DIVIDE;assert(skill.Calculate(1,0,100)==-80);
 data->m_bInc=SVI_PRECENT;assert(skill.Calculate(1,0,100)==-95);
 assert(skill.IsDamageType() && skill.GetAttackType()==SAT_PHYSIC);
 TSKILLDATA same=*data;assert(skill.HaveSkillData(&same));same.m_bExec=0;assert(!skill.HaveSkillData(&same));
 assert(skill.GetAggro(2)==8 && skill.GetPrice(200)==100);
 assert(!skill.GetNeedSkillPoint(3) && !skill.CheckParentSkill(3,99));
 auto* point=new TSKILLPOINT();point->m_bSkillPoint=4;point->m_bGroupPoint=5;point->m_bParentLevel=2;
 skill.m_mapTSkillPoint.emplace(3,point);
 assert(skill.GetNeedSkillPoint(3)==4 && skill.GetNeedKindPoint(3)==5);
 assert(!skill.CheckParentSkill(3,1) && skill.CheckParentSkill(3,2));
 data->m_bType=SDT_TRANS;data->m_bExec=SDT_TRANS_DISGUISE_D;
 assert(!skill.CheckCountry(TCONTRY_D) && skill.CheckCountry(TCONTRY_C));
 data->m_bType=SDT_STATUS;data->m_bExec=SDT_STATUS_AUTOEXP;data->m_wValue=12;
 assert(skill.IsExpPotion() && skill.GetAutoExp()==12);
 data->m_bAction=SA_BUFF;assert(skill.IsBuffType() && skill.IsMaintainType());
 skill.m_bPositive=SPT_POSITIVE;assert(skill.IsPositive());
 std::cout<<"PASS original Map skill calculations, classifications, eligibility, point lookup and populated ownership\n";
}
