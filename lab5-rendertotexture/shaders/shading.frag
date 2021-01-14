#version 420

// required by GLSL spec Sect 4.5.3 (though nvidia does not, amd does)
precision highp float;

///////////////////////////////////////////////////////////////////////////////
// Material
///////////////////////////////////////////////////////////////////////////////
uniform vec3 material_color;
uniform float material_reflectivity;
uniform float material_metalness;
uniform float material_fresnel;
uniform float material_shininess;
uniform float material_emission;

uniform int has_color_texture;
layout(binding = 0) uniform sampler2D colorMap;
uniform int has_emission_texture;
layout(binding = 5) uniform sampler2D emissiveMap;

///////////////////////////////////////////////////////////////////////////////
// Environment
///////////////////////////////////////////////////////////////////////////////
layout(binding = 6) uniform sampler2D environmentMap;
layout(binding = 7) uniform sampler2D irradianceMap;
layout(binding = 8) uniform sampler2D reflectionMap;
uniform float environment_multiplier;

///////////////////////////////////////////////////////////////////////////////
// Light source
///////////////////////////////////////////////////////////////////////////////
uniform vec3 point_light_color = vec3(1.0, 1.0, 1.0);
uniform float point_light_intensity_multiplier = 50.0;

///////////////////////////////////////////////////////////////////////////////
// Constants
///////////////////////////////////////////////////////////////////////////////
#define PI 3.14159265359

///////////////////////////////////////////////////////////////////////////////
// Input varyings from vertex shader
///////////////////////////////////////////////////////////////////////////////
in vec2 texCoord;
in vec3 viewSpaceNormal;
in vec3 viewSpacePosition;

///////////////////////////////////////////////////////////////////////////////
// Input uniform variables
///////////////////////////////////////////////////////////////////////////////
uniform mat4 viewInverse;
uniform vec3 viewSpaceLightPosition;

///////////////////////////////////////////////////////////////////////////////
// Output color
///////////////////////////////////////////////////////////////////////////////
layout(location = 0) out vec4 fragmentColor;


vec3 calculateDirectIllumiunation(vec3 wo, vec3 n, vec3 base_color)
{
//vec3 direct_illum = base_color;
	vec3 wi = normalize(viewSpaceLightPosition-viewSpacePosition);

	///////////////////////////////////////////////////////////////////////////
	// Task 1.2 - Calculate the radiance Li from the light, and the direction
	//            to the light. If the light is backfacing the triangle,
	//            return vec3(0);
	///////////////////////////////////////////////////////////////////////////
	float d = distance(viewSpacePosition,viewSpaceLightPosition);
	vec3 Li = point_light_intensity_multiplier * point_light_color * 1/(d*d);

	if(dot((viewSpaceNormal),(viewSpaceLightPosition-viewSpacePosition)) <= 0) {
		return vec3(0.0);
	}
	
	///////////////////////////////////////////////////////////////////////////
	// Task 1.3 - Calculate the diffuse term and return that as the result
	///////////////////////////////////////////////////////////////////////////
	// vec3 diffuse_term = ...

	vec3 diffuse_term = material_color * 1/PI * abs(dot(n,wi)) * Li;

	///////////////////////////////////////////////////////////////////////////
	// Task 2 - Calculate the Torrance Sparrow BRDF and return the light
	//          reflected from that instead
	///////////////////////////////////////////////////////////////////////////
	vec3 wh = normalize(wi + wo);
	float F = material_fresnel + (1-material_fresnel) * pow(max(1-dot(wh,wi),0.001),5);
	float Dwh = (material_shininess + 2)/(2*PI) * pow(dot(n,wh),material_shininess);
	float G = min(1,min(2*(dot(n,wh)*dot(n,wo))/dot(wo,wh),2*(dot(n,wh)*dot(n,wi))/dot(wo,wh)));
	float brdf = (F*Dwh*G)/max((4*dot(n,wo)*dot(n,wi)),0.001);
	///////////////////////////////////////////////////////////////////////////
	// Task 3 - Make your shader respect the parameters of our material model.
	///////////////////////////////////////////////////////////////////////////
	vec3 dielectric_term = brdf * dot(n,wi) * Li + (1-F) * diffuse_term;
	vec3 metal_term = brdf * material_color * dot(n,wi) * Li;
	vec3 microfacet_term = material_metalness * metal_term + (1-material_metalness) * dielectric_term;

	return material_reflectivity * microfacet_term + (1-material_reflectivity) * diffuse_term;}

vec3 calculateIndirectIllumination(vec3 wo, vec3 n, vec3 base_color)
{
vec3 indirect_illum = vec3(0.f);
	
	///////////////////////////////////////////////////////////////////////////
	// Task 5 - Lookup the irradiance from the irradiance map and calculate
	//          the diffuse reflection
	///////////////////////////////////////////////////////////////////////////
	vec4 nW = viewInverse * vec4(n,0.0);

	// Calculate the spherical coordinates of the direction
	float theta = acos(max(-1.0f, min(1.0f, nW.y)));
	float phi = atan(nW.z, nW.x);
	if(phi < 0.0f)
	{
		phi = phi + 2.0f * PI;
	}

	// Use these to lookup the color in the environment map
	vec2 lookup = vec2(phi / (2.0 * PI), theta / PI);


	//is this really the irradience??
	vec3 irradience = environment_multiplier * texture(irradianceMap,lookup).xyz;

	vec3 diffuse_term = material_color * 1/PI * irradience;
	//return diffuse_term;

	///////////////////////////////////////////////////////////////////////////
	// Task 6 - Look up in the reflection map from the perfect specular
	//          direction and calculate the dielectric and metal terms.
	///////////////////////////////////////////////////////////////////////////
	vec3 wi = reflect(-wo,n);
	vec4 wiWorld = viewInverse * vec4(wi,0.0);
	float roughness = sqrt(sqrt(2/(material_shininess+2)));

	// Calculate the spherical coordinates of the direction
	theta = acos(max(-1.0f, min(1.0f, wiWorld.y)));
	phi = atan(wiWorld.z, wiWorld.x);
	if(phi < 0.0f)
	{
		phi = phi + 2.0f * PI;
	}

	// Use these to lookup the color in the environment map
	lookup = vec2(phi / (2.0 * PI), theta / PI);
	vec3 Li = environment_multiplier * textureLod(reflectionMap,lookup,roughness * 7.0).xyz;

	vec3 wh = normalize(wi + wo);
	float F = material_fresnel + (1-material_fresnel) * pow(max(1-dot(wo,wh),0.001),5);

	vec3 dielectric_term = F * Li + (1-F) * diffuse_term;
	vec3 metal_term = F * material_color * Li;
	vec3 microfacet_term = material_metalness * metal_term + (1-material_metalness) * dielectric_term;

	return material_reflectivity * microfacet_term + (1-material_reflectivity) * diffuse_term;
}


void main()
{
	vec3 wo = -normalize(viewSpacePosition);
	vec3 n = normalize(viewSpaceNormal);

	vec3 base_color = material_color;
	if(has_color_texture == 1)
	{
		base_color *= texture(colorMap, texCoord).xyz;
	}

	// Direct illumination
	vec3 direct_illumination_term = calculateDirectIllumiunation(wo, n, base_color);

	// Indirect illumination
	vec3 indirect_illumination_term = calculateIndirectIllumination(wo, n, base_color);

	///////////////////////////////////////////////////////////////////////////
	// Add emissive term. If emissive texture exists, sample this term.
	///////////////////////////////////////////////////////////////////////////
	vec3 emission_term = material_emission * material_color;
	if(has_emission_texture == 1)
	{
		emission_term *= texture(emissiveMap, texCoord).xyz;
	}

	fragmentColor.xyz = direct_illumination_term + indirect_illumination_term + emission_term;
}
