#include "qwsvdef.h"

void Mod_LoadBrushModel (model_t *mod, void *buffer);

void
Mod_LoadAliasModel(model_t *mod, void *buf)
{
	Mod_LoadBrushModel (mod, buf);
}

void
Mod_LoadSpriteModel(model_t *mod, void *buf)
{
	Mod_LoadBrushModel (mod, buf);
}

void
R_InitSky(struct texture_s *mt)
{
}