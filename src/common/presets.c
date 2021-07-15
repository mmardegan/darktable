/*
    This file is part of darktable,
    Copyright (C) 2019-2020 darktable developers.

    darktable is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    darktable is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with darktable.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "common/presets.h"
#include "common/darktable.h"
#include "common/debug.h"
#include "common/exif.h"
#include "common/file_location.h"
#include "develop/blend.h"
#include "develop/imageop.h"
#include "libs/lib.h"

#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include <glib.h>
#include <inttypes.h>
#include <sqlite3.h>

static char *dt_preset_encode(sqlite3_stmt *stmt, int row)
{
  const int32_t len = sqlite3_column_bytes(stmt, row);
  char *vparams = dt_exif_xmp_encode((const unsigned char *)sqlite3_column_blob(stmt, row), len, NULL);
  return vparams;
}

void dt_presets_save_to_file(const int rowid, const char *preset_name, const char *filedir)
{
  sqlite3_stmt *stmt;

  // generate filename based on name of preset
  // convert all characters to underscore which are not allowed in filenames
  gchar *presetname = g_strdup(preset_name);
  gchar *filename = g_strdup_printf("%s/%s.dtpreset", filedir, g_strdelimit(presetname, "/<>:\"\\|*?[]", '_'));

  g_free(presetname);

  DT_DEBUG_SQLITE3_PREPARE_V2(dt_database_get(darktable.db),
                              "SELECT op_params, blendop_params, name, description, operation,"
                              "   autoapply, model, maker, lens, iso_min, iso_max, exposure_min,"
                              "   exposure_max, aperture_min, aperture_max, focal_length_min,"
                              "   focal_length_max, op_version, blendop_version, enabled,"
                              "   multi_priority, multi_name, filter, def, format "
                              " FROM data.presets"
                              " WHERE rowid = ?1",
                              -1, &stmt, NULL);
  DT_DEBUG_SQLITE3_BIND_INT(stmt, 1, rowid);

  if(sqlite3_step(stmt) == SQLITE_ROW)
  {
    const gchar *name = (gchar *)sqlite3_column_text(stmt, 2);
    const gchar *description = (gchar *)sqlite3_column_text(stmt, 3);
    const gchar *operation = (gchar *)sqlite3_column_text(stmt, 4);
    const int autoapply = sqlite3_column_int(stmt, 5);
    const gchar *model = (gchar *)sqlite3_column_text(stmt, 6);
    const gchar *maker = (gchar *)sqlite3_column_text(stmt, 7);
    const gchar *lens = (gchar *)sqlite3_column_text(stmt, 8);
    const float iso_min = sqlite3_column_double(stmt, 9);
    const float iso_max = sqlite3_column_double(stmt, 10);
    const float exposure_min = sqlite3_column_double(stmt, 11);
    const float exposure_max = sqlite3_column_double(stmt, 12);
    const float aperture_min = sqlite3_column_double(stmt, 13);
    const float aperture_max = sqlite3_column_double(stmt, 14);
    const int focal_length_min = sqlite3_column_double(stmt, 15);
    const int focal_length_max = sqlite3_column_double(stmt, 16);
    const int op_version = sqlite3_column_int(stmt, 17);
    const int blendop_version = sqlite3_column_int(stmt, 18);
    const int enabled = sqlite3_column_int(stmt, 19);
    const int multi_priority = sqlite3_column_int(stmt, 20);
    const gchar *multi_name = (gchar *)sqlite3_column_text(stmt, 21);
    const int filter = sqlite3_column_double(stmt, 22);
    const int def = sqlite3_column_double(stmt, 23);
    const int format = sqlite3_column_double(stmt, 24);

    int rc = 0;

    xmlTextWriterPtr writer = xmlNewTextWriterFilename(filename, 0);

    if(writer == NULL)
    {
      fprintf(stderr, "[dt_presets_save_to_file] Error creating the xml writer\n, path: %s", filename);
      g_free(filename);
      return;
    }

    rc = xmlTextWriterStartDocument(writer, NULL, "UTF-8", NULL);
    if(rc < 0)
    {
      fprintf(stderr, "[dt_presets_save_to_file]: Error on encoding setting");
      g_free(filename);
      return;
    }

    xmlTextWriterStartElement(writer, BAD_CAST "darktable_preset");
    xmlTextWriterWriteAttribute(writer, BAD_CAST "version", BAD_CAST "1.0");

    xmlTextWriterStartElement(writer, BAD_CAST "preset");
    xmlTextWriterWriteFormatElement(writer, BAD_CAST "name", "%s", name);
    xmlTextWriterWriteFormatElement(writer, BAD_CAST "description", "%s", description);
    xmlTextWriterWriteFormatElement(writer, BAD_CAST "operation", "%s", operation);
    xmlTextWriterWriteFormatElement(writer, BAD_CAST "op_params", "%s", dt_preset_encode(stmt, 0));
    xmlTextWriterWriteFormatElement(writer, BAD_CAST "op_version", "%d", op_version);
    xmlTextWriterWriteFormatElement(writer, BAD_CAST "enabled", "%d", enabled);
    xmlTextWriterWriteFormatElement(writer, BAD_CAST "autoapply", "%d", autoapply);
    xmlTextWriterWriteFormatElement(writer, BAD_CAST "model", "%s", model);
    xmlTextWriterWriteFormatElement(writer, BAD_CAST "maker", "%s", maker);
    xmlTextWriterWriteFormatElement(writer, BAD_CAST "lens", "%s", lens);
    xmlTextWriterWriteFormatElement(writer, BAD_CAST "iso_min", "%f", iso_min);
    xmlTextWriterWriteFormatElement(writer, BAD_CAST "iso_max", "%f", iso_max);
    xmlTextWriterWriteFormatElement(writer, BAD_CAST "exposure_min", "%f", exposure_min);
    xmlTextWriterWriteFormatElement(writer, BAD_CAST "exposure_max", "%f", exposure_max);
    xmlTextWriterWriteFormatElement(writer, BAD_CAST "aperture_min", "%f", aperture_min);
    xmlTextWriterWriteFormatElement(writer, BAD_CAST "aperture_max", "%f", aperture_max);
    xmlTextWriterWriteFormatElement(writer, BAD_CAST "focal_length_min", "%d", focal_length_min);
    xmlTextWriterWriteFormatElement(writer, BAD_CAST "focal_length_max", "%d", focal_length_max);
    xmlTextWriterWriteFormatElement(writer, BAD_CAST "blendop_params", "%s", dt_preset_encode(stmt, 1));
    xmlTextWriterWriteFormatElement(writer, BAD_CAST "blendop_version", "%d", blendop_version);
    xmlTextWriterWriteFormatElement(writer, BAD_CAST "multi_priority", "%d", multi_priority);
    xmlTextWriterWriteFormatElement(writer, BAD_CAST "multi_name", "%s", multi_name);
    xmlTextWriterWriteFormatElement(writer, BAD_CAST "filter", "%d", filter);
    xmlTextWriterWriteFormatElement(writer, BAD_CAST "def", "%d", def);
    xmlTextWriterWriteFormatElement(writer, BAD_CAST "format", "%d", format);
    xmlTextWriterEndElement(writer);

    sqlite3_finalize(stmt);
    xmlTextWriterEndDocument(writer);
    xmlFreeTextWriter(writer);
  }
  g_free(filename);
}

static gchar *get_preset_element(xmlDocPtr doc, gchar *name)
{
  xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);
  char xpath[128] = { 0 };
  snprintf(xpath, sizeof(xpath), "//%s", name);
  gchar *result = NULL;

  xmlXPathObjectPtr xpathObj =
    xmlXPathEvalExpression((const xmlChar *)xpath, xpathCtx);

  if(xpathObj)
  {
    const xmlNodeSetPtr xnodes = xpathObj->nodesetval;
    if(xnodes->nodeTab)
    {
      const xmlNodePtr xnode = xnodes->nodeTab[0];
      xmlChar *value = xmlNodeListGetString(doc, xnode->xmlChildrenNode, 1);

      if(value)
        result = g_strdup((gchar *)value);
      else
        result = g_strdup("");

      xmlFree(value);
    }
    xmlXPathFreeObject(xpathObj);
  }

  xmlXPathFreeContext(xpathCtx);
  return result;
}

static int get_preset_element_int(xmlDocPtr doc, gchar *name)
{
  gchar *value = get_preset_element(doc, name);
  const int result = value ? atoi(value) : 0;
  g_free(value);
  return result;
}

static int get_preset_element_float(xmlDocPtr doc, gchar *name)
{
  gchar *value = get_preset_element(doc, name);
  const float result = value ? atof(value) : 0.0f;
  g_free(value);
  return result;
}

int dt_presets_import_from_file(const char *preset_path)
{
  xmlDocPtr doc = xmlParseFile(preset_path);
  if(!doc)
    return FALSE;

  gchar *name = get_preset_element(doc, "name");
  gchar *description = get_preset_element(doc, "description");
  gchar *operation = get_preset_element(doc, "operation");
  const int autoapply = get_preset_element_int(doc, "autoapply");
  gchar *model = get_preset_element(doc, "model");
  gchar *maker = get_preset_element(doc, "maker");
  gchar *lens = get_preset_element(doc, "lens");
  const float iso_min = get_preset_element_float(doc, "iso_min");
  const float iso_max = get_preset_element_float(doc, "iso_max");
  const float exposure_min = get_preset_element_float(doc, "exposure_min");
  const float exposure_max = get_preset_element_float(doc, "exposure_max");
  const float aperture_min = get_preset_element_float(doc, "aperture_min");
  const float aperture_max = get_preset_element_float(doc, "aperture_max");
  const int focal_length_min = get_preset_element_int(doc, "focal_length_min");
  const int focal_length_max = get_preset_element_int(doc, "focal_length_max");
  gchar *op_params = get_preset_element(doc, "op_params");
  const int op_version = get_preset_element_int(doc, "op_version");
  gchar *blendop_params = get_preset_element(doc, "blendop_params");
  const int blendop_version = get_preset_element_int(doc, "blendop_version");
  const int enabled = get_preset_element_int(doc, "enabled");
  const int multi_priority = get_preset_element_int(doc, "multi_priority");
  gchar *multi_name = get_preset_element(doc, "multi_name");
  const int filter = get_preset_element_int(doc, "filter");
  const int def = get_preset_element_int(doc, "def");
  const int format = get_preset_element_int(doc, "format");
  xmlFreeDoc(doc);

  int blendop_params_len = 0;
  const unsigned char *blendop_params_blob = dt_exif_xmp_decode
    (blendop_params, strlen(blendop_params), &blendop_params_len);

  int op_params_len = 0;
  const unsigned char *op_params_blob = dt_exif_xmp_decode
    (op_params, strlen(op_params), &op_params_len);

  sqlite3_stmt *stmt;
  int result = 0;

  DT_DEBUG_SQLITE3_PREPARE_V2(dt_database_get(darktable.db),
     "INSERT OR REPLACE"
     "  INTO data.presets"
     "    (name, description, operation, autoapply,"
     "     model, maker, lens, iso_min, iso_max, exposure_min, exposure_max,"
     "     aperture_min, aperture_max, focal_length_min, focal_length_max,"
     "     op_params, op_version, blendop_params, blendop_version, enabled,"
     "     multi_priority, multi_name, filter, def, format, writeprotect)"
     "  VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14, "
     "          ?15, ?16, ?17, ?18, ?19, ?20, ?21, ?22, ?23, ?24, ?25, 0)",
     -1, &stmt, NULL);

  DT_DEBUG_SQLITE3_BIND_TEXT(stmt, 1, name, strlen(name), SQLITE_TRANSIENT);
  DT_DEBUG_SQLITE3_BIND_TEXT(stmt, 2, description, strlen(description), SQLITE_TRANSIENT);
  DT_DEBUG_SQLITE3_BIND_TEXT(stmt, 3, operation, strlen(operation), SQLITE_TRANSIENT);
  DT_DEBUG_SQLITE3_BIND_INT(stmt, 4, autoapply);
  DT_DEBUG_SQLITE3_BIND_TEXT(stmt, 5, model, strlen(model), SQLITE_TRANSIENT);
  DT_DEBUG_SQLITE3_BIND_TEXT(stmt, 6, maker, strlen(maker), SQLITE_TRANSIENT);
  DT_DEBUG_SQLITE3_BIND_TEXT(stmt, 7, lens, strlen(lens), SQLITE_TRANSIENT);
  DT_DEBUG_SQLITE3_BIND_DOUBLE(stmt, 8, iso_min);
  DT_DEBUG_SQLITE3_BIND_DOUBLE(stmt, 9, iso_max);
  DT_DEBUG_SQLITE3_BIND_DOUBLE(stmt, 10, exposure_min);
  DT_DEBUG_SQLITE3_BIND_DOUBLE(stmt, 11, exposure_max);
  DT_DEBUG_SQLITE3_BIND_DOUBLE(stmt, 12, aperture_min);
  DT_DEBUG_SQLITE3_BIND_DOUBLE(stmt, 13, aperture_max);
  DT_DEBUG_SQLITE3_BIND_INT(stmt, 14, focal_length_min);
  DT_DEBUG_SQLITE3_BIND_INT(stmt, 15, focal_length_max);
  DT_DEBUG_SQLITE3_BIND_BLOB(stmt, 16, op_params_blob, op_params_len, SQLITE_TRANSIENT);
  DT_DEBUG_SQLITE3_BIND_INT(stmt, 17, op_version);
  DT_DEBUG_SQLITE3_BIND_BLOB(stmt, 18, blendop_params_blob, blendop_params_len, SQLITE_TRANSIENT);
  DT_DEBUG_SQLITE3_BIND_INT(stmt, 19, blendop_version);
  DT_DEBUG_SQLITE3_BIND_INT(stmt, 20, enabled);
  DT_DEBUG_SQLITE3_BIND_INT(stmt, 21, multi_priority);
  DT_DEBUG_SQLITE3_BIND_TEXT(stmt, 22, multi_name, strlen(multi_name), SQLITE_TRANSIENT);
  DT_DEBUG_SQLITE3_BIND_INT(stmt, 23, filter);
  DT_DEBUG_SQLITE3_BIND_INT(stmt, 24, def);
  DT_DEBUG_SQLITE3_BIND_INT(stmt, 25, format);

  result = (sqlite3_step(stmt) == SQLITE_OK);

  sqlite3_finalize(stmt);

  g_free(name);
  g_free(description);
  g_free(operation);
  g_free(model);
  g_free(maker);
  g_free(lens);
  g_free(op_params);
  g_free(blendop_params);
  g_free(multi_name);

  return result;
}

gboolean dt_presets_module_can_autoapply(const gchar *operation)
{
  for(const GList *lib_modules = darktable.lib->plugins; lib_modules; lib_modules = g_list_next(lib_modules))
  {
    dt_lib_module_t *lib_module = (dt_lib_module_t *)lib_modules->data;
    if(!strcmp(lib_module->plugin_name, operation))
    {
      return dt_lib_presets_can_autoapply(lib_module);
    }
  }
  return TRUE;
}
// modelines: These editor modelines have been set for all relevant files by tools/update_modelines.sh
// vim: shiftwidth=2 expandtab tabstop=2 cindent
// kate: tab-indents: off; indent-width 2; replace-tabs on; indent-mode cstyle; remove-trailing-spaces modified;
