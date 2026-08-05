#pragma once
/*

___________________________________


offline init

jungle USD
v
cooker
v
[StaticScene]

___________________________________


online init

[StaticScene]
v
SceneBoundsBuilder
v
[SceneBounds]
v
SceneResourcesTempBuilder
v
[SceneResourcesTemp]
v
SceneResourcesBuilder
SceneResourcesTextureBuilder
v
[SceneResources]

___________________________________

per frame

[SceneResourcesTemp] [Camera]
v
SceneDynamicDataBuilder
v
[DynamicSceneData]

___________________________________

per frame

[Camera]
v
SceneFrameConstDataBuilder
v
[FrameConstData]

___________________________________


final: SceneResources, DynamicSceneData, FrameConstData

*/