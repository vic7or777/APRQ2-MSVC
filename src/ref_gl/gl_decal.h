void GL_ClearDecals (void);
void R_AddDecals (void);
void R_Decals_Init(void);

void R_AddDecal (vec3_t origin, vec3_t dir,
				 float red, float green, float blue, float alpha,
				 float size, int type, int flags, float angle);
