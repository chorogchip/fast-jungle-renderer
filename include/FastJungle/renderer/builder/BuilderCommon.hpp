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
SceneDrawBuilder
v
[SceneDraws]
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

[SceneDraws] [Camera]
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
