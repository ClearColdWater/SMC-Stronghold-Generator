#pragma once

#include"StrongholdSMC.hpp"

namespace StrongholdSMC
{
    bool StrongholdBatch::generateStrongholdsWithBootstrapInfo(
        const StrongholdObservation* strongholdObservation, uint32_t epochs,
        uint32_t initN, double resampleThreshold, uint64_t seed,
        const StrongholdAuxiliaryInfo* guidingStrongholdInfo // in case multiple runs was needed
    ){
        StrongholdAuxiliaryInfo info;

        if(guidingStrongholdInfo != nullptr)info = *guidingStrongholdInfo;
        else info.makeInfoFromObservation(strongholdObservation);
        
        for(uint64_t i = 0; i < epochs; i++)
        {
            bool success;

            if(i == epochs - 1)
            {
                std::cerr << "generating " << initN << " strongholds..." << std::endl;
                success = this->generateStrongholds(strongholdObservation,            initN, resampleThreshold, seed + 0x7fffffff * i, 1024, 3, &info);
            }
            else
            {
                std::cerr << "generating " << initN << " strongholds for bootstrap info, epoch " << i << "..." << std::endl;
                success = this->generateStrongholds(strongholdObservation, (initN >> 3) + 1, resampleThreshold, seed + 0x7fffffff * i,  768, 1, &info);
            }
                
            if(!success)
                return false;

            info = this->getNextInfo();
        }
        return true;
    }
}