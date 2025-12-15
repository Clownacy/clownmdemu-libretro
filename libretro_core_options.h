#ifndef LIBRETRO_CORE_OPTIONS_H__
#define LIBRETRO_CORE_OPTIONS_H__

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "libretro.h"

#define HAVE_NO_LANGEXTRA

#ifndef HAVE_NO_LANGEXTRA
#include "libretro_core_options_intl.h"
#endif

/*
 ********************************
 * VERSION: 2.0
 ********************************
 *
 * - 2.0: Add support for core options v2 interface
 * - 1.3: Move translations to libretro_core_options_intl.h
 *        - libretro_core_options_intl.h includes BOM and utf-8
 *          fix for MSVC 2010-2013
 *        - Added HAVE_NO_LANGEXTRA flag to disable translations
 *          on platforms/compilers without BOM support
 * - 1.2: Use core options v1 interface when
 *        RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION is >= 1
 *        (previously required RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION == 1)
 * - 1.1: Support generation of core options v0 retro_core_option_value
 *        arrays containing options with a single value
 * - 1.0: First commit
*/

#ifdef __cplusplus
extern "C" {
#endif

/*
 ********************************
 * Core Option Definitions
 ********************************
*/

/* RETRO_LANGUAGE_ENGLISH */

/* Default language:
 * - All other languages must include the same keys and values
 * - Will be used as a fallback in the event that frontend language
 *   is not available
 * - Will be used as a fallback for any missing entries in
 *   frontend language definition */

struct retro_core_option_v2_category option_cats_us[] = {
	{
		/* Key. */
		"debug",
		/* Label. */
		"Debug",
		/* Description. */
		"Unusual options that are intended for debugging."
	},
	{
		/* Key. */
		"console",
		/* Label. */
		"Console",
		/* Description. */
		"Options related to the emulated console."
	},
	{
		/* Key. */
		"video",
		/* Label. */
		"Video",
		/* Description. */
		"Options related to graphical operations."
	},
	{
		/* Key. */
		"audio",
		/* Label. */
		"Audio",
		/* Description. */
		"Options related to sound operations."
	},
	{NULL, NULL, NULL}
};

struct retro_core_option_v2_definition option_defs_us[] = {
	{
		/* Key. */
		"clownmdemu_disable_sprite_plane",
		/* Label. */
		"Debug > Disable Sprite Plane",
		/* Categorised label. */
		"Disable Sprite Plane",
		/* Description. */
		"Disable the VDP's Sprite Plane.",
		/* Categorised description. */
		NULL,
		/* Category. */
		"debug",
		/* Values. */
		{
			{"enabled", NULL},
			{"disabled", NULL},
			{NULL, NULL},
		},
		/* Default value. */
		"disabled"
	},
	{
		/* Key. */
		"clownmdemu_disable_window_plane",
		/* Label. */
		"Debug > Disable Window Plane",
		/* Categorised label. */
		"Disable Window Plane",
		/* Description. */
		"Disable the VDP's Window Plane.",
		/* Categorised description. */
		NULL,
		/* Category. */
		"debug",
		/* Values. */
		{
			{"enabled", NULL},
			{"disabled", NULL},
			{NULL, NULL},
		},
		/* Default value. */
		"disabled"
	},
	{
		/* Key. */
		"clownmdemu_disable_plane_a",
		/* Label. */
		"Debug > Disable Plane A",
		/* Categorised label. */
		"Disable Plane A",
		/* Description. */
		"Disable the VDP's Plane A.",
		/* Categorised description. */
		NULL,
		/* Category. */
		"debug",
		/* Values. */
		{
			{"enabled", NULL},
			{"disabled", NULL},
			{NULL, NULL},
		},
		/* Default value. */
		"disabled"
	},
	{
		/* Key. */
		"clownmdemu_disable_plane_b",
		/* Label. */
		"Debug > Disable Plane B",
		/* Categorised label. */
		"Disable Plane B",
		/* Description. */
		"Disable the VDP's Plane B.",
		/* Categorised description. */
		NULL,
		/* Category. */
		"debug",
		/* Values. */
		{
			{"enabled", NULL},
			{"disabled", NULL},
			{NULL, NULL},
		},
		/* Default value. */
		"disabled"
	},
	{
		/* Key. */
		"clownmdemu_disable_fm1",
		/* Label. */
		"Debug > Disable FM1",
		/* Categorised label. */
		"Disable FM1",
		/* Description. */
		"Disable the YM2612's FM1 channel.",
		/* Categorised description. */
		NULL,
		/* Category. */
		"debug",
		/* Values. */
		{
			{"enabled", NULL},
			{"disabled", NULL},
			{NULL, NULL},
		},
		/* Default value. */
		"disabled"
	},
	{
		/* Key. */
		"clownmdemu_disable_fm2",
		/* Label. */
		"Debug > Disable FM2",
		/* Categorised label. */
		"Disable FM2",
		/* Description. */
		"Disable the YM2612's FM2 channel.",
		/* Categorised description. */
		NULL,
		/* Category. */
		"debug",
		/* Values. */
		{
			{"enabled", NULL},
			{"disabled", NULL},
			{NULL, NULL},
		},
		/* Default value. */
		"disabled"
	},
	{
		/* Key. */
		"clownmdemu_disable_fm3",
		/* Label. */
		"Debug > Disable FM3",
		/* Categorised label. */
		"Disable FM3",
		/* Description. */
		"Disable the YM2612's FM3 channel.",
		/* Categorised description. */
		NULL,
		/* Category. */
		"debug",
		/* Values. */
		{
			{"enabled", NULL},
			{"disabled", NULL},
			{NULL, NULL},
		},
		/* Default value. */
		"disabled"
	},
	{
		/* Key. */
		"clownmdemu_disable_fm4",
		/* Label. */
		"Debug > Disable FM4",
		/* Categorised label. */
		"Disable FM4",
		/* Description. */
		"Disable the YM2612's FM4 channel.",
		/* Categorised description. */
		NULL,
		/* Category. */
		"debug",
		/* Values. */
		{
			{"enabled", NULL},
			{"disabled", NULL},
			{NULL, NULL},
		},
		/* Default value. */
		"disabled"
	},
	{
		/* Key. */
		"clownmdemu_disable_fm5",
		/* Label. */
		"Debug > Disable FM5",
		/* Categorised label. */
		"Disable FM5",
		/* Description. */
		"Disable the YM2612's FM5 channel.",
		/* Categorised description. */
		NULL,
		/* Category. */
		"debug",
		/* Values. */
		{
			{"enabled", NULL},
			{"disabled", NULL},
			{NULL, NULL},
		},
		/* Default value. */
		"disabled"
	},
	{
		/* Key. */
		"clownmdemu_disable_fm6",
		/* Label. */
		"Debug > Disable FM6",
		/* Categorised label. */
		"Disable FM6",
		/* Description. */
		"Disable the YM2612's FM6 channel.",
		/* Categorised description. */
		NULL,
		/* Category. */
		"debug",
		/* Values. */
		{
			{"enabled", NULL},
			{"disabled", NULL},
			{NULL, NULL},
		},
		/* Default value. */
		"disabled"
	},
	{
		/* Key. */
		"clownmdemu_disable_dac",
		/* Label. */
		"Debug > Disable DAC",
		/* Categorised label. */
		"Disable DAC",
		/* Description. */
		"Disable the YM2612's DAC channel.",
		/* Categorised description. */
		NULL,
		/* Category. */
		"debug",
		/* Values. */
		{
			{"enabled", NULL},
			{"disabled", NULL},
			{NULL, NULL},
		},
		/* Default value. */
		"disabled"
	},
	{
		/* Key. */
		"clownmdemu_disable_psg1",
		/* Label. */
		"Debug > Disable PSG1",
		/* Categorised label. */
		"Disable PSG1",
		/* Description. */
		"Disable the SN76496's PSG1 channel.",
		/* Categorised description. */
		NULL,
		/* Category. */
		"debug",
		/* Values. */
		{
			{"enabled", NULL},
			{"disabled", NULL},
			{NULL, NULL},
		},
		/* Default value. */
		"disabled"
	},
	{
		/* Key. */
		"clownmdemu_disable_psg2",
		/* Label. */
		"Debug > Disable PSG2",
		/* Categorised label. */
		"Disable PSG2",
		/* Description. */
		"Disable the SN76496's PSG2 channel.",
		/* Categorised description. */
		NULL,
		/* Category. */
		"debug",
		/* Values. */
		{
			{"enabled", NULL},
			{"disabled", NULL},
			{NULL, NULL},
		},
		/* Default value. */
		"disabled"
	},
	{
		/* Key. */
		"clownmdemu_disable_psg3",
		/* Label. */
		"Debug > Disable PSG3",
		/* Categorised label. */
		"Disable PSG3",
		/* Description. */
		"Disable the SN76496's PSG3 channel.",
		/* Categorised description. */
		NULL,
		/* Category. */
		"debug",
		/* Values. */
		{
			{"enabled", NULL},
			{"disabled", NULL},
			{NULL, NULL},
		},
		/* Default value. */
		"disabled"
	},
	{
		/* Key. */
		"clownmdemu_disable_psg_noise",
		/* Label. */
		"Debug > Disable PSG Noise",
		/* Categorised label. */
		"Disable PSG Noise",
		/* Description. */
		"Disable the SN76496's PSG Noise channel.",
		/* Categorised description. */
		NULL,
		/* Category. */
		"debug",
		/* Values. */
		{
			{"enabled", NULL},
			{"disabled", NULL},
			{NULL, NULL},
		},
		/* Default value. */
		"disabled"
	},
#define DO_PCM_CHANNEL(NUMBER) \
	{ \
		/* Key. */ \
		"clownmdemu_disable_pcm" NUMBER, \
		/* Label. */ \
		"Debug > Disable PCM" NUMBER, \
		/* Categorised label. */ \
		"Disable PCM" NUMBER, \
		/* Description. */ \
		"Disable the RF5C164's PCM" NUMBER " channel.", \
		/* Categorised description. */ \
		NULL, \
		/* Category. */ \
		"debug", \
		/* Values. */ \
		{ \
			{"enabled", NULL}, \
			{"disabled", NULL}, \
			{NULL, NULL}, \
		}, \
		/* Default value. */ \
		"disabled" \
	},
	DO_PCM_CHANNEL("1")
	DO_PCM_CHANNEL("2")
	DO_PCM_CHANNEL("3")
	DO_PCM_CHANNEL("4")
	DO_PCM_CHANNEL("5")
	DO_PCM_CHANNEL("6")
	DO_PCM_CHANNEL("7")
	DO_PCM_CHANNEL("8")
#undef DO_PCM_CHANNEL
	{
		/* Key. */
		"clownmdemu_tv_standard",
		/* Label. */
		"Console > TV Standard",
		/* Categorised label. */
		"TV Standard",
		/* Description. */
		"Which television standard to output in.",
		/* Categorised description. */
		NULL,
		/* Category. */
		"console",
		/* Values. */
		{
			{"pal", "PAL (50Hz)"},
			{"ntsc", "NTSC (59.94Hz)"},
			{NULL, NULL},
		},
		/* Default value. */
		"ntsc"
	},
	{
		/* Key. */
		"clownmdemu_overseas_region",
		/* Label. */
		"Console > Region",
		/* Categorised label. */
		"Region",
		/* Description. */
		"Which region the console is.",
		/* Categorised description. */
		NULL,
		/* Category. */
		"console",
		/* Values. */
		{
			{"elsewhere", "Overseas (Elsewhere)"},
			{"japan", "Domestic (Japan)"},
			{NULL, NULL},
		},
		/* Default value. */
		"elsewhere"
	},
	{
		/* Key. */
		"clownmdemu_cd_addon",
		/* Label. */
		"Console > CD Add-on",
		/* Categorised label. */
		"CD Add-on",
		/* Description. */
		"Allow cartridge-only software to utilise features of the emulated Mega CD add-on, such as CD music. This may break some software.",
		/* Categorised description. */
		NULL,
		/* Category. */
		"console",
		/* Values. */
		{
			{"enabled", NULL},
			{"disabled", NULL},
			{NULL, NULL},
		},
		/* Default value. */
		"disabled"
	},
	{
		/* Key. */
		"clownmdemu_tall_interlace_mode_2",
		/* Label. */
		"Video > Tall Interlace Mode 2",
		/* Categorised label. */
		"Tall Interlace Mode 2",
		/* Description. */
		"Makes games that use Interlace Mode 2 for split-screen not appear squashed.",
		/* Categorised description. */
		NULL,
		/* Category. */
		"video",
		/* Values. */
		{
			{"enabled", NULL},
			{"disabled", NULL},
			{NULL, NULL},
		},
		/* Default value. */
		"disabled"
	},
	{
		/* Key. */
		"clownmdemu_widescreen",
		/* Label. */
		"Video > Widescreen Hack",
		/* Categorised label. */
		"Widescreen Hack",
		/* Description. */
		"Widens the display. Works well with some games, badly with others.",
		/* Categorised description. */
		NULL,
		/* Category. */
		"video",
		/* Values. */
		{
			{"enabled", NULL},
			{"disabled", NULL},
			{NULL, NULL},
		},
		/* Default value. */
		"disabled"
	},
	{
		/* Key. */
		"clownmdemu_lowpass_filter",
		/* Label. */
		"Audio > Low-Pass Filter",
		/* Categorised label. */
		"Low-Pass Filter",
		/* Description. */
		"Makes the audio sound 'softer', just like on a real Mega Drive.",
		/* Categorised description. */
		NULL,
		/* Category. */
		"audio",
		/* Values. */
		{
			{"enabled", NULL},
			{"disabled", NULL},
			{NULL, NULL},
		},
		/* Default value. */
		"enabled"
	},
	{
		/* Key. */
		"clownmdemu_ladder_effect",
		/* Label. */
		"Audio > Low-Volume Distortion",
		/* Categorised label. */
		"Low-Volume Distortion",
		/* Description. */
		"Approximates the so-called 'ladder effect' that is present in early Mega Drives. Without this, certain sounds in some games will be too quiet.",
		/* Categorised description. */
		NULL,
		/* Category. */
		"audio",
		/* Values. */
		{
			{"enabled", NULL},
			{"disabled", NULL},
			{NULL, NULL},
		},
		/* Default value. */
		"enabled"
	},
	{NULL, NULL, NULL, NULL, NULL, NULL, {{NULL, NULL}}, NULL}
};

struct retro_core_options_v2 options_us = {
   option_cats_us,
   option_defs_us
};

/*
 ********************************
 * Language Mapping
 ********************************
*/

#ifndef HAVE_NO_LANGEXTRA
struct retro_core_options_v2 *options_intl[RETRO_LANGUAGE_LAST] = {
   &options_us, /* RETRO_LANGUAGE_ENGLISH */
   NULL,        /* RETRO_LANGUAGE_JAPANESE */
   &options_fr, /* RETRO_LANGUAGE_FRENCH */
   NULL,        /* RETRO_LANGUAGE_SPANISH */
   NULL,        /* RETRO_LANGUAGE_GERMAN */
   NULL,        /* RETRO_LANGUAGE_ITALIAN */
   NULL,        /* RETRO_LANGUAGE_DUTCH */
   NULL,        /* RETRO_LANGUAGE_PORTUGUESE_BRAZIL */
   NULL,        /* RETRO_LANGUAGE_PORTUGUESE_PORTUGAL */
   NULL,        /* RETRO_LANGUAGE_RUSSIAN */
   NULL,        /* RETRO_LANGUAGE_KOREAN */
   NULL,        /* RETRO_LANGUAGE_CHINESE_TRADITIONAL */
   NULL,        /* RETRO_LANGUAGE_CHINESE_SIMPLIFIED */
   NULL,        /* RETRO_LANGUAGE_ESPERANTO */
   NULL,        /* RETRO_LANGUAGE_POLISH */
   NULL,        /* RETRO_LANGUAGE_VIETNAMESE */
   NULL,        /* RETRO_LANGUAGE_ARABIC */
   NULL,        /* RETRO_LANGUAGE_GREEK */
   NULL,        /* RETRO_LANGUAGE_TURKISH */
   NULL,        /* RETRO_LANGUAGE_SLOVAK */
   NULL,        /* RETRO_LANGUAGE_PERSIAN */
   NULL,        /* RETRO_LANGUAGE_HEBREW */
   NULL,        /* RETRO_LANGUAGE_ASTURIAN */
   NULL,        /* RETRO_LANGUAGE_FINNISH */
   NULL,        /* RETRO_LANGUAGE_INDONESIAN */
   NULL,        /* RETRO_LANGUAGE_SWEDISH */
   NULL,        /* RETRO_LANGUAGE_UKRAINIAN */
   NULL,        /* RETRO_LANGUAGE_CZECH */
};
#endif

/*
 ********************************
 * Functions
 ********************************
*/

/* Handles configuration/setting of core options.
 * Should be called as early as possible - ideally inside
 * retro_set_environment(), and no later than retro_load_game()
 * > We place the function body in the header to avoid the
 *   necessity of adding more .c files (i.e. want this to
 *   be as painless as possible for core devs)
 */

static void libretro_set_core_options(retro_environment_t environ_cb)
{
   unsigned version  = 0;
#ifndef HAVE_NO_LANGEXTRA
   unsigned language = 0;
#endif

   if (!environ_cb(RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION, &version))
      version = 0;

   if (version >= 2)
   {
#ifndef HAVE_NO_LANGEXTRA
      struct retro_core_options_v2_intl core_options_intl;

      core_options_intl.us    = &options_us;
      core_options_intl.local = NULL;

      if (environ_cb(RETRO_ENVIRONMENT_GET_LANGUAGE, &language) &&
          (language < RETRO_LANGUAGE_LAST) && (language != RETRO_LANGUAGE_ENGLISH))
         core_options_intl.local = options_intl[language];

      environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL, &core_options_intl);
#else
      environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2, &options_us);
#endif
   }
   else
   {
      size_t i, j;
      size_t option_index              = 0;
      size_t num_options               = 0;
      struct retro_core_option_definition
            *option_v1_defs_us         = NULL;
#ifndef HAVE_NO_LANGEXTRA
      size_t num_options_intl          = 0;
      struct retro_core_option_v2_definition
            *option_defs_intl          = NULL;
      struct retro_core_option_definition
            *option_v1_defs_intl       = NULL;
      struct retro_core_options_intl
            core_options_v1_intl;
#endif
      struct retro_variable *variables = NULL;
      char **values_buf                = NULL;

      /* Determine total number of options */
      while (true)
      {
         if (option_defs_us[num_options].key)
            num_options++;
         else
            break;
      }

      if (version >= 1)
      {
         /* Allocate US array */
         option_v1_defs_us = (struct retro_core_option_definition *)
               calloc(num_options + 1, sizeof(struct retro_core_option_definition));

         /* Copy parameters from option_defs_us array */
         for (i = 0; i < num_options; i++)
         {
            struct retro_core_option_v2_definition *option_def_us = &option_defs_us[i];
            struct retro_core_option_value *option_values         = option_def_us->values;
            struct retro_core_option_definition *option_v1_def_us = &option_v1_defs_us[i];
            struct retro_core_option_value *option_v1_values      = option_v1_def_us->values;

            option_v1_def_us->key           = option_def_us->key;
            option_v1_def_us->desc          = option_def_us->desc;
            option_v1_def_us->info          = option_def_us->info;
            option_v1_def_us->default_value = option_def_us->default_value;

            /* Values must be copied individually... */
            while (option_values->value)
            {
               option_v1_values->value = option_values->value;
               option_v1_values->label = option_values->label;

               option_values++;
               option_v1_values++;
            }
         }

#ifndef HAVE_NO_LANGEXTRA
         if (environ_cb(RETRO_ENVIRONMENT_GET_LANGUAGE, &language) &&
             (language < RETRO_LANGUAGE_LAST) && (language != RETRO_LANGUAGE_ENGLISH) &&
             options_intl[language])
            option_defs_intl = options_intl[language]->definitions;

         if (option_defs_intl)
         {
            /* Determine number of intl options */
            while (true)
            {
               if (option_defs_intl[num_options_intl].key)
                  num_options_intl++;
               else
                  break;
            }

            /* Allocate intl array */
            option_v1_defs_intl = (struct retro_core_option_definition *)
                  calloc(num_options_intl + 1, sizeof(struct retro_core_option_definition));

            /* Copy parameters from option_defs_intl array */
            for (i = 0; i < num_options_intl; i++)
            {
               struct retro_core_option_v2_definition *option_def_intl = &option_defs_intl[i];
               struct retro_core_option_value *option_values           = option_def_intl->values;
               struct retro_core_option_definition *option_v1_def_intl = &option_v1_defs_intl[i];
               struct retro_core_option_value *option_v1_values        = option_v1_def_intl->values;

               option_v1_def_intl->key           = option_def_intl->key;
               option_v1_def_intl->desc          = option_def_intl->desc;
               option_v1_def_intl->info          = option_def_intl->info;
               option_v1_def_intl->default_value = option_def_intl->default_value;

               /* Values must be copied individually... */
               while (option_values->value)
               {
                  option_v1_values->value = option_values->value;
                  option_v1_values->label = option_values->label;

                  option_values++;
                  option_v1_values++;
               }
            }
         }

         core_options_v1_intl.us    = option_v1_defs_us;
         core_options_v1_intl.local = option_v1_defs_intl;

         environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL, &core_options_v1_intl);
#else
         environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS, option_v1_defs_us);
#endif
      }
      else
      {
         /* Allocate arrays */
         variables  = (struct retro_variable *)calloc(num_options + 1,
               sizeof(struct retro_variable));
         values_buf = (char **)calloc(num_options, sizeof(char *));

         if (!variables || !values_buf)
            goto error;

         /* Copy parameters from option_defs_us array */
         for (i = 0; i < num_options; i++)
         {
            const char *key                        = option_defs_us[i].key;
            const char *desc                       = option_defs_us[i].desc;
            const char *default_value              = option_defs_us[i].default_value;
            struct retro_core_option_value *values = option_defs_us[i].values;
            size_t buf_len                         = 3;
            size_t default_index                   = 0;

            values_buf[i] = NULL;

            if (desc)
            {
               size_t num_values = 0;

               /* Determine number of values */
               while (true)
               {
                  if (values[num_values].value)
                  {
                     /* Check if this is the default value */
                     if (default_value)
                        if (strcmp(values[num_values].value, default_value) == 0)
                           default_index = num_values;

                     buf_len += strlen(values[num_values].value);
                     num_values++;
                  }
                  else
                     break;
               }

               /* Build values string */
               if (num_values > 0)
               {
                  buf_len += num_values - 1;
                  buf_len += strlen(desc);

                  values_buf[i] = (char *)calloc(buf_len, sizeof(char));
                  if (!values_buf[i])
                     goto error;

                  strcpy(values_buf[i], desc);
                  strcat(values_buf[i], "; ");

                  /* Default value goes first */
                  strcat(values_buf[i], values[default_index].value);

                  /* Add remaining values */
                  for (j = 0; j < num_values; j++)
                  {
                     if (j != default_index)
                     {
                        strcat(values_buf[i], "|");
                        strcat(values_buf[i], values[j].value);
                     }
                  }
               }
            }

            variables[option_index].key   = key;
            variables[option_index].value = values_buf[i];
            option_index++;
         }

         /* Set variables */
         environ_cb(RETRO_ENVIRONMENT_SET_VARIABLES, variables);
      }

error:
      /* Clean up */

      if (option_v1_defs_us)
      {
         free(option_v1_defs_us);
         option_v1_defs_us = NULL;
      }

#ifndef HAVE_NO_LANGEXTRA
      if (option_v1_defs_intl)
      {
         free(option_v1_defs_intl);
         option_v1_defs_intl = NULL;
      }
#endif

      if (values_buf)
      {
         for (i = 0; i < num_options; i++)
         {
            if (values_buf[i])
            {
               free(values_buf[i]);
               values_buf[i] = NULL;
            }
         }

         free(values_buf);
         values_buf = NULL;
      }

      if (variables)
      {
         free(variables);
         variables = NULL;
      }
   }
}

#ifdef __cplusplus
}
#endif

#endif
