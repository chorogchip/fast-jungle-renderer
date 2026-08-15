#include "SWRasterCommon.hlsli"

[numthreads(SOFTWARE_THREADS_PER_GROUP, 1, 1)]
void main(
    uint thread_id : SV_GroupThreadID,
    uint3 group_id : SV_GroupID)
{
    SoftwareRasterMain(thread_id, group_id);
}
