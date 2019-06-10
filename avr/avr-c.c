/* Copyright (C) 2009-2015 Free Software Foundation, Inc.
   Contributed by Anatoly Sokolov (aesok@post.ru)

   This file is part of GCC.

   GCC is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.
   
   GCC is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with GCC; see the file COPYING3.  If not see
   <http://www.gnu.org/licenses/>.  */

/* Not included in avr.c since this requires C front end.  */

#include "avr-device-config.h"

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "tm_p.h"
#include "cpplib.h"
#include "hash-set.h"
#include "machmode.h"
#include "vec.h"
#include "double-int.h"
#include "input.h"
#include "alias.h"
#include "symtab.h"
#include "wide-int.h"
#include "inchash.h"
#include "tree.h"
#include "stor-layout.h"
#include "target.h"
#include "c-family/c-common.h"
#include "langhooks.h"

/* IDs for all the AVR builtins.  */

enum avr_builtin_id
  {
#define DEF_BUILTIN(NAME, N_ARGS, TYPE, CODE, LIBNAME)  \
    AVR_BUILTIN_ ## NAME,
#include "builtins.def"
#undef DEF_BUILTIN

    AVR_BUILTIN_COUNT
  };


/* Implement `TARGET_RESOLVE_OVERLOADED_PLUGIN'.  */

static tree
avr_resolve_overloaded_builtin (unsigned int iloc, tree fndecl, void *vargs)
{
  tree type0, type1, fold = NULL_TREE;
  enum avr_builtin_id id = AVR_BUILTIN_COUNT;
  location_t loc = (location_t) iloc;
  vec<tree, va_gc> &args = * (vec<tree, va_gc>*) vargs;

  switch (DECL_FUNCTION_CODE (fndecl))
    {
    default:
      break;

    case AVR_BUILTIN_ABSFX:
      if (args.length() != 1)
        {
          error_at (loc, "%qs expects 1 argument but %d given",
                    "absfx", (int) args.length());

          fold = error_mark_node;
          break;
        }

      type0 = TREE_TYPE (args[0]);

      if (!FIXED_POINT_TYPE_P (type0))
        {
          error_at (loc, "%qs expects a fixed-point value as argument",
                    "absfx");

          fold = error_mark_node;
        }

      switch (TYPE_MODE (type0))
        {
        case QQmode: id = AVR_BUILTIN_ABSHR; break;
        case HQmode: id = AVR_BUILTIN_ABSR; break;
        case SQmode: id = AVR_BUILTIN_ABSLR; break;
        case DQmode: id = AVR_BUILTIN_ABSLLR; break;

        case HAmode: id = AVR_BUILTIN_ABSHK; break;
        case SAmode: id = AVR_BUILTIN_ABSK; break;
        case DAmode: id = AVR_BUILTIN_ABSLK; break;
        case TAmode: id = AVR_BUILTIN_ABSLLK; break;

        case UQQmode:
        case UHQmode:
        case USQmode:
        case UDQmode:
        case UHAmode:
        case USAmode:
        case UDAmode:
        case UTAmode:
          warning_at (loc, 0, "using %qs with unsigned type has no effect",
                      "absfx");
          return args[0];

        default:
          error_at (loc, "no matching fixed-point overload found for %qs",
                    "absfx");

          fold = error_mark_node;
          break;
        }

      fold = targetm.builtin_decl (id, true);

      if (fold != error_mark_node)
        fold = build_function_call_vec (loc, vNULL, fold, &args, NULL);

      break; // absfx

    case AVR_BUILTIN_ROUNDFX:
      if (args.length() != 2)
        {
          error_at (loc, "%qs expects 2 arguments but %d given",
                    "roundfx", (int) args.length());

          fold = error_mark_node;
          break;
        }

      type0 = TREE_TYPE (args[0]);
      type1 = TREE_TYPE (args[1]);

      if (!FIXED_POINT_TYPE_P (type0))
        {
          error_at (loc, "%qs expects a fixed-point value as first argument",
                    "roundfx");

          fold = error_mark_node;
        }

      if (!INTEGRAL_TYPE_P (type1))
        {
          error_at (loc, "%qs expects an integer value as second argument",
                    "roundfx");

          fold = error_mark_node;
        }

      switch (TYPE_MODE (type0))
        {
        case QQmode: id = AVR_BUILTIN_ROUNDHR; break;
        case HQmode: id = AVR_BUILTIN_ROUNDR; break;
        case SQmode: id = AVR_BUILTIN_ROUNDLR; break;
        case DQmode: id = AVR_BUILTIN_ROUNDLLR; break;

        case UQQmode: id = AVR_BUILTIN_ROUNDUHR; break;
        case UHQmode: id = AVR_BUILTIN_ROUNDUR; break;
        case USQmode: id = AVR_BUILTIN_ROUNDULR; break;
        case UDQmode: id = AVR_BUILTIN_ROUNDULLR; break;

        case HAmode: id = AVR_BUILTIN_ROUNDHK; break;
        case SAmode: id = AVR_BUILTIN_ROUNDK; break;
        case DAmode: id = AVR_BUILTIN_ROUNDLK; break;
        case TAmode: id = AVR_BUILTIN_ROUNDLLK; break;

        case UHAmode: id = AVR_BUILTIN_ROUNDUHK; break;
        case USAmode: id = AVR_BUILTIN_ROUNDUK; break;
        case UDAmode: id = AVR_BUILTIN_ROUNDULK; break;
        case UTAmode: id = AVR_BUILTIN_ROUNDULLK; break;

        default:
          error_at (loc, "no matching fixed-point overload found for %qs",
                    "roundfx");

          fold = error_mark_node;
          break;
        }

      fold = targetm.builtin_decl (id, true);

      if (fold != error_mark_node)
        fold = build_function_call_vec (loc, vNULL, fold, &args, NULL);

      break; // roundfx

    case AVR_BUILTIN_COUNTLSFX:
      if (args.length() != 1)
        {
          error_at (loc, "%qs expects 1 argument but %d given",
                    "countlsfx", (int) args.length());

          fold = error_mark_node;
          break;
        }

      type0 = TREE_TYPE (args[0]);

      if (!FIXED_POINT_TYPE_P (type0))
        {
          error_at (loc, "%qs expects a fixed-point value as first argument",
                    "countlsfx");

          fold = error_mark_node;
        }

      switch (TYPE_MODE (type0))
        {
        case QQmode: id = AVR_BUILTIN_COUNTLSHR; break;
        case HQmode: id = AVR_BUILTIN_COUNTLSR; break;
        case SQmode: id = AVR_BUILTIN_COUNTLSLR; break;
        case DQmode: id = AVR_BUILTIN_COUNTLSLLR; break;

        case UQQmode: id = AVR_BUILTIN_COUNTLSUHR; break;
        case UHQmode: id = AVR_BUILTIN_COUNTLSUR; break;
        case USQmode: id = AVR_BUILTIN_COUNTLSULR; break;
        case UDQmode: id = AVR_BUILTIN_COUNTLSULLR; break;

        case HAmode: id = AVR_BUILTIN_COUNTLSHK; break;
        case SAmode: id = AVR_BUILTIN_COUNTLSK; break;
        case DAmode: id = AVR_BUILTIN_COUNTLSLK; break;
        case TAmode: id = AVR_BUILTIN_COUNTLSLLK; break;

        case UHAmode: id = AVR_BUILTIN_COUNTLSUHK; break;
        case USAmode: id = AVR_BUILTIN_COUNTLSUK; break;
        case UDAmode: id = AVR_BUILTIN_COUNTLSULK; break;
        case UTAmode: id = AVR_BUILTIN_COUNTLSULLK; break;

        default:
          error_at (loc, "no matching fixed-point overload found for %qs",
                    "countlsfx");

          fold = error_mark_node;
          break;
        }

      fold = targetm.builtin_decl (id, true);

      if (fold != error_mark_node)
        fold = build_function_call_vec (loc, vNULL, fold, &args, NULL);

      break; // countlsfx
    }

  return fold;
}
  
/* Implement `REGISTER_TARGET_PRAGMAS'.  */
extern void c_register_pragma (const char * space, const char * name,
                               void (* callback ) (struct cpp_reader *));
void
avr_register_target_pragmas (void)
{
  int i;

  gcc_assert (ADDR_SPACE_GENERIC == ADDR_SPACE_RAM);

  /* Register address spaces.  The order must be the same as in the respective
     enum from avr.h (or designated initializers must be used in avr.c).  */

  for (i = 0; i < ADDR_SPACE_COUNT; i++)
    {
      gcc_assert (i == avr_addrspace[i].id);

      if (!ADDR_SPACE_GENERIC_P (i))
        c_register_addr_space (avr_addrspace[i].name, avr_addrspace[i].id);
    }

  targetm.resolve_overloaded_builtin = avr_resolve_overloaded_builtin;

  // Register actual target pragmas
  c_register_pragma(0, "config", avr_handle_config_pragma);
}


/* Transform LO into uppercase and write the result to UP.
   You must provide enough space for UP.  Return UP.  */

static char*
avr_toupper (char *up, const char *lo)
{
  char *up0 = up;

  for (; *lo; lo++, up++)
    *up = TOUPPER (*lo);

  *up = '\0';

  return up0;
}

extern HOST_WIDE_INT mchp_avr_license_valid;

/* Worker function for TARGET_CPU_CPP_BUILTINS.  */

void
avr_cpu_cpp_builtins (struct cpp_reader *pfile)
{
  int i;

  builtin_define_std ("AVR");

  /* __AVR_DEVICE_NAME__ and  avr_mcu_types[].macro like __AVR_ATmega8__
     are defined by -D command option, see device-specs file.  */

  if (avr_arch->macro)
    cpp_define_formatted (pfile, "__AVR_ARCH__=%s", avr_arch->macro);
  if (AVR_HAVE_RAMPD)    cpp_define (pfile, "__AVR_HAVE_RAMPD__");
  if (AVR_HAVE_RAMPX)    cpp_define (pfile, "__AVR_HAVE_RAMPX__");
  if (AVR_HAVE_RAMPY)    cpp_define (pfile, "__AVR_HAVE_RAMPY__");
  if (AVR_HAVE_RAMPZ)    cpp_define (pfile, "__AVR_HAVE_RAMPZ__");
  if (AVR_HAVE_ELPM)     cpp_define (pfile, "__AVR_HAVE_ELPM__");
  if (AVR_HAVE_ELPMX)    cpp_define (pfile, "__AVR_HAVE_ELPMX__");
  if (AVR_HAVE_MOVW)     cpp_define (pfile, "__AVR_HAVE_MOVW__");
  if (AVR_HAVE_LPMX)     cpp_define (pfile, "__AVR_HAVE_LPMX__");

  if (avr_arch->asm_only)
    cpp_define (pfile, "__AVR_ASM_ONLY__");
  if (AVR_HAVE_MUL)
    {
      cpp_define (pfile, "__AVR_ENHANCED__");
      cpp_define (pfile, "__AVR_HAVE_MUL__");
    }

  if (AVR_HAVE_JMP_CALL)
    cpp_define (pfile, "__AVR_HAVE_JMP_CALL__");

  if (avr_arch->have_jmp_call)
    cpp_define (pfile, "__AVR_MEGA__");

  if (AVR_SHORT_CALLS)
    cpp_define (pfile, "__AVR_SHORT_CALLS__");

  if (AVR_XMEGA)
    cpp_define (pfile, "__AVR_XMEGA__");

  if (AVR_TINY)
    {
      cpp_define (pfile, "__AVR_TINY__");

      /* Define macro "__AVR_TINY_PM_BASE_ADDRESS__" with mapped program memory
         start address.  This macro shall be used where mapped program
         memory is accessed, eg. copying data section (__do_copy_data)
         contents to data memory region.
         NOTE:
         Program memory of AVR_TINY devices cannot be accessed directly,
         it has been mapped to the data memory.  For AVR_TINY devices
         (ATtiny4/5/9/10/20 and 40) mapped program memory starts at 0x4000. */

      cpp_define_formatted (pfile, "__AVR_TINY_PM_BASE_ADDRESS__=0x%x",
                            avr_arch->flash_pm_offset);
    }

  if (avr_arch->flash_pm_offset)
    cpp_define_formatted (pfile, "__AVR_PM_BASE_ADDRESS__=0x%x",
                          avr_arch->flash_pm_offset);

  if (AVR_HAVE_EIJMP_EICALL)
    {
      cpp_define (pfile, "__AVR_HAVE_EIJMP_EICALL__");
      cpp_define (pfile, "__AVR_3_BYTE_PC__");
    }
  else
    {
      cpp_define (pfile, "__AVR_2_BYTE_PC__");
    }

  if (AVR_HAVE_8BIT_SP)
    cpp_define (pfile, "__AVR_HAVE_8BIT_SP__");
  else
    cpp_define (pfile, "__AVR_HAVE_16BIT_SP__");

  if (AVR_HAVE_SPH)
    cpp_define (pfile, "__AVR_HAVE_SPH__");
  else
    cpp_define (pfile, "__AVR_SP8__");

  if (TARGET_NO_INTERRUPTS)
    cpp_define (pfile, "__NO_INTERRUPTS__");

  if (TARGET_SKIP_BUG)
    {
      cpp_define (pfile, "__AVR_ERRATA_SKIP__");

      if (AVR_HAVE_JMP_CALL)
        cpp_define (pfile, "__AVR_ERRATA_SKIP_JMP_CALL__");
    }

  if (TARGET_RMW)
    cpp_define (pfile, "__AVR_ISA_RMW__");

  if (TARGET_CCI)
    cpp_define (pfile, "__CCI__");

  cpp_define_formatted (pfile, "__AVR_SFR_OFFSET__=0x%x",
                        avr_arch->sfr_offset);

#ifdef WITH_AVRLIBC
  cpp_define (pfile, "__WITH_AVRLIBC__");
#endif /* WITH_AVRLIBC */

  /* Define builtin macros so that the user can easily query whether
     non-generic address spaces (and which) are supported or not.
     This is only supported for C.  For C++, a language extension is needed
     (as mentioned in ISO/IEC DTR 18037; Annex F.2) which is not
     implemented in GCC up to now.  */

  if (lang_GNU_C ())
    {
      for (i = 0; i < ADDR_SPACE_COUNT; i++)
        if (!ADDR_SPACE_GENERIC_P (i)
            /* Only supply __FLASH<n> macro if the address space is reasonable
               for this target.  The address space qualifier itself is still
               supported, but using it will throw an error.  */
            && avr_addrspace[i].segment < avr_n_flash
	    /* Only support __MEMX macro if we have LPM.  */
	    && (AVR_HAVE_LPM || avr_addrspace[i].pointer_size <= 2))

          {
            const char *name = avr_addrspace[i].name;
            char *Name = (char*) alloca (1 + strlen (name));

            cpp_define (pfile, avr_toupper (Name, name));
          }
    }

  /* Define macro to denote const data allocation in program memory.  */
  if (avr_const_data_in_progmem)
    {
      cpp_define (pfile, "__AVR_CONST_DATA_IN_PROGMEM__");
    }

  /* Define macro to denote program memory access from data address space.  */
  if (AVR_HAVE_PROGMEM_IN_DATA_ADDRESS_SPACE)
    {
      cpp_define (pfile, "__AVR_PROGMEM_IN_DATA_ADDRESS_SPACE__");
    }

  if (AVR_CONST_DATA_IN_MEMX_ADDRESS_SPACE)
    {
      cpp_define (pfile, "__AVR_CONST_DATA_IN_MEMX_ADDRESS_SPACE__");
    }
	

  /* Define builtin macros so that the user can easily query whether or
     not a specific builtin is available. */

#define DEF_BUILTIN(NAME, N_ARGS, TYPE, CODE, LIBNAME)  \
  cpp_define (pfile, "__BUILTIN_AVR_" #NAME);
#include "builtins.def"
#undef DEF_BUILTIN

  /* Builtin macros for the __int24 and __uint24 type.  */

  cpp_define_formatted (pfile, "__INT24_MAX__=8388607%s",
                        INT_TYPE_SIZE == 8 ? "LL" : "L");
  cpp_define (pfile, "__INT24_MIN__=(-__INT24_MAX__-1)");
  cpp_define_formatted (pfile, "__UINT24_MAX__=16777215%s",
                        INT_TYPE_SIZE == 8 ? "ULL" : "UL");

  cpp_define_formatted (pfile, "XC8_MODE=%d", mchp_avr_license_valid);

}

//#include "avr-device-config.cpp"

extern enum cpp_ttype pragma_lex (tree *value);

#ifndef CLEAR_REST_OF_INPUT_LINE
#define CLEAR_REST_OF_INPUT_LINE() \
  do {                             \
       int t;                      \
       tree tv;                    \
       do {                        \
         t=pragma_lex(&tv);        \
       } while (t!=CPP_EOF);       \
      } while(0);
#endif

/* handler function for pragma config.  */
void
avr_handle_config_pragma (struct cpp_reader *pfile)
{
  enum cpp_ttype tok;
  tree tok_value;
  static int shown_no_config_warning = 0;
  
  if (flag_generate_lto)
    {
      warning (0, "#pragma config not supported with -flto option, this file will not participate in LTO");
      flag_generate_lto = 0;
    }

  if (DeviceConfigurations.ConfigFile.empty())
    {
      error ("#pragma config directive not available as device config file "
             "not found.");
      CLEAR_REST_OF_INPUT_LINE();
      return;
    }

   /* if configurations are not loaded, warn and skip pragma config.  */
	if (!DeviceConfigurations.AreConfigsLoaded)
    {
      if (!shown_no_config_warning)
        {
          shown_no_config_warning = 1;
          warning (0, "Configuration word information not available for "
                   "this device. #pragma config is ignored.");
        }
        CLEAR_REST_OF_INPUT_LINE();
        return;
    }

  /* The payload for the config pragma is a comma delimited list of
     "setting = value" pairs. Both the setting and the value must
     be valid C identifiers. */
  tok = pragma_lex (&tok_value);
  while (1)
    {
      const cpp_token *raw_token;
      const char *setting_name = NULL;
      unsigned char *value_name = NULL;

      /* the current token should be the setting name */
      if (tok != CPP_NAME)
        {
          error ("configuration setting name expected in configuration pragma");
          break;
        }

      setting_name = IDENTIFIER_POINTER (tok_value);
      /* the next token should be the '=' */
      tok = pragma_lex (&tok_value);
      if (tok != CPP_EQ)
        {
          error ("'=' expected in configuration pragma");
          break;
        }
      /* now we have the value name. We don't use pragma_lex() to get this one
         since we don't want the additional interpretation going on there.
         i.e., converting integers from the string. */
      tok = pragma_lex (&tok_value);
      if (tok == CPP_NAME)
        value_name = (unsigned char *)IDENTIFIER_POINTER (tok_value);
      else if (tok == CPP_NUMBER)
        {
          /* transform the token to number if no error in lex  */
          if ((tok_value != error_mark_node) && (tree_fits_uhwi_p (tok_value)))
          {
            #define MAX_VALUE_NAME_LENGTH 22
            HOST_WIDE_INT i;
            i = tree_to_uhwi(tok_value);//, 1 /* positive only */ );
            value_name = (unsigned char*)xcalloc(MAX_VALUE_NAME_LENGTH,1);
            snprintf((char *)value_name, MAX_VALUE_NAME_LENGTH, "%d", i);
            #undef MAX_VALUE_NAME_LENGTH
          }
        }
      else
        {
          error ("config-setting value must be a name or a constant integer");
          break;
        }

      /* get the next (look ahead) token. */
      tok = pragma_lex (&tok_value);

      /* issue error if we have spurious input.  */
      if ((tok != CPP_COMMA) && (tok != CPP_EOF))
        {
          error ("',' or end of line expected in configuration pragma");
          break;
        }

      /* Set the configuration if we have setting name and value.  */
      char ErrorMsg[127] = "";
      if ((setting_name && value_name) &&
          !DeviceConfigurations.SetConfig(std::string(setting_name),
                                          std::string((const char*)value_name),
                                          ErrorMsg))
        {
          error ("Error in setting config (%s)", ErrorMsg);
          break;
        }

      /* Continue if the next token is comma, otherwise we are done.  */
      if (tok == CPP_COMMA)
        {
          tok = pragma_lex (&tok_value);
          continue;
        }
      else
        {
          gcc_assert (tok == CPP_EOF);
          break;
        }
    }
  /* if we ended for any reason other than end of line, we have an error.
     Any needed diagnostic should have already been issued, so just
     clear the rest of the data on the line. */
  if (tok != CPP_EOF)
    CLEAR_REST_OF_INPUT_LINE();
}

