/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2016 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 2.1
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 2.1 of the license, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib.h>
#include <glib/gprintf.h>

#include "appstream.h"
#include "as-xml.h"
#include "as-component-private.h"
#include "as-test-utils.h"

static gchar *datadir = NULL;

/**
 * test_screenshot_handling:
 *
 * Test reading screenshot tags.
 */
static void
test_screenshot_handling ()
{
	AsMetadata *metad;
	GError *error = NULL;
	AsComponent *cpt;
	GFile *file;
	gchar *path;
	GPtrArray *screenshots;
	guint i;

	metad = as_metadata_new ();
	as_metadata_set_format_style (metad, AS_FORMAT_STYLE_COLLECTION);

	path = g_build_filename (datadir, "appstream-dxml.xml", NULL);
	file = g_file_new_for_path (path);
	g_free (path);

	as_metadata_parse_file (metad, file, AS_FORMAT_KIND_XML, &error);
	g_object_unref (file);
	g_assert_no_error (error);

	cpt = as_metadata_get_component (metad);
	g_assert (cpt != NULL);

	// dirty...
	g_debug ("%s", as_component_to_string (cpt));
	screenshots = as_component_get_screenshots (cpt);
	g_assert_cmpint (screenshots->len, >, 0);

	for (i = 0; i < screenshots->len; i++) {
		GPtrArray *imgs;
		AsScreenshot *sshot = (AsScreenshot*) g_ptr_array_index (screenshots, i);

		imgs = as_screenshot_get_images (sshot);
		g_assert_cmpint (imgs->len, ==, 2);
		g_debug ("%s", as_screenshot_get_caption (sshot));
	}

	g_object_unref (metad);
}

/**
 * test_appstream_parser_legacy:
 *
 * Test parsing legacy metainfo files.
 */
static void
test_appstream_parser_legacy ()
{
	AsMetadata *metad;
	GFile *file;
	gchar *path;
	AsComponent *cpt;
	GPtrArray *screenshots;
	GError *error = NULL;

	metad = as_metadata_new ();

	path = g_build_filename (datadir, "appdata-legacy.xml", NULL);
	file = g_file_new_for_path (path);
	g_free (path);

	as_metadata_parse_file (metad, file, AS_FORMAT_KIND_XML, &error);
	cpt = as_metadata_get_component (metad);
	g_object_unref (file);
	g_assert_no_error (error);
	g_assert (cpt != NULL);

	g_assert_cmpstr (as_component_get_summary (cpt), ==, "Application manager for GNOME");
	g_assert (as_component_get_kind (cpt) == AS_COMPONENT_KIND_DESKTOP_APP);

	screenshots = as_component_get_screenshots (cpt);
	g_assert_cmpint (screenshots->len, ==, 5);

	g_object_unref (metad);
}

/**
 * test_appstream_parser_locale:
 *
 * Test reading localized tags.
 */
static void
test_appstream_parser_locale ()
{
	g_autoptr(AsMetadata) metad = NULL;
	g_autoptr(GFile) file = NULL;
	gchar *path;
	AsComponent *cpt;

	GPtrArray *trs;
	AsTranslation *tr;

	GError *error = NULL;

	metad = as_metadata_new ();

	path = g_build_filename (datadir, "appdata.xml", NULL);
	file = g_file_new_for_path (path);
	g_free (path);

	/* check german only locale */
	as_metadata_set_locale (metad, "de_DE");
	as_metadata_parse_file (metad, file, AS_FORMAT_KIND_XML, &error);
	cpt = as_metadata_get_component (metad);
	g_assert_no_error (error);
	g_assert (cpt != NULL);

	g_assert (as_component_get_kind (cpt) == AS_COMPONENT_KIND_DESKTOP_APP);
	g_assert_cmpstr (as_component_get_name (cpt), ==, "Feuerfuchs");
	as_component_set_active_locale (cpt, "C");
	g_assert_cmpstr (as_component_get_name (cpt), ==, "Firefox");
	/* no french, so fallback */
	as_component_set_active_locale (cpt, "fr_FR");
	g_assert_cmpstr (as_component_get_name (cpt), ==, "Firefox");

	/* check all locale */
	as_metadata_clear_components (metad);
	as_metadata_set_locale (metad, "ALL");
	as_metadata_parse_file (metad, file, AS_FORMAT_KIND_XML, &error);
	cpt = as_metadata_get_component (metad);
	g_assert_no_error (error);

	as_component_set_active_locale (cpt, "C");
	g_assert_cmpstr (as_component_get_name (cpt), ==, "Firefox");
	as_component_set_active_locale (cpt, "de_DE");
	g_assert_cmpstr (as_component_get_name (cpt), ==, "Feuerfuchs");
	/* no french, so fallback */
	as_component_set_active_locale (cpt, "fr_FR");
	g_assert_cmpstr (as_component_get_name (cpt), ==, "Firefoux");

	/* check if reading <translation/> tag succeeded */
	trs = as_component_get_translations (cpt);
	g_assert_cmpint (trs->len, ==, 1);
	tr = AS_TRANSLATION (g_ptr_array_index (trs, 0));
	g_assert_cmpstr (as_translation_get_id (tr), ==, "firefox");

	/* check if we loaded the right amount of icons */
	g_assert_cmpint (as_component_get_icons (cpt)->len, ==, 2);
}

/**
 * test_appstream_write_locale:
 *
 * Test writing fully localized entries.
 */
static void
test_appstream_write_locale ()
{
	AsMetadata *metad;
	GFile *file;
	gchar *tmp;
	AsComponent *cpt;
	GError *error = NULL;

	const gchar *EXPECTED_XML = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
				    "<component type=\"desktop-application\">\n"
				    "  <id>firefox.desktop</id>\n"
				    "  <name xml:lang=\"fr_FR\">Firefoux</name>\n"
				    "  <name>Firefox</name>\n"
				    "  <name xml:lang=\"de_DE\">Feuerfuchs</name>\n"
				    "  <summary xml:lang=\"fr_FR\">Navigateur web</summary>\n"
				    "  <summary>Web browser</summary>\n"
				    "  <pkgname>firefox-bin</pkgname>\n"
				    "  <categories>\n"
				    "    <category>network</category>\n"
				    "    <category>web</category>\n"
				    "  </categories>\n"
				    "  <keywords>\n"
				    "    <keyword>internet</keyword>\n"
				    "    <keyword>web</keyword>\n"
				    "    <keyword>browser</keyword>\n"
				    "    <keyword>navigateur</keyword>\n"
				    "  </keywords>\n"
				    "  <url type=\"homepage\">http://www.mozilla.com</url>\n"
				    "  <icon type=\"stock\">web-browser</icon>\n"
				    "  <icon type=\"cached\" width=\"64\" height=\"64\">firefox_web-browser.png</icon>\n"
				    "  <translation type=\"gettext\">firefox</translation>\n"
				    "  <mimetypes>\n"
				    "    <mimetype>application/vnd.mozilla.xul+xml</mimetype>\n"
				    "    <mimetype>application/x-xpinstall</mimetype>\n"
				    "    <mimetype>application/xhtml+xml</mimetype>\n"
				    "    <mimetype>text/html</mimetype>\n"
				    "    <mimetype>text/mml</mimetype>\n"
				    "    <mimetype>text/xml</mimetype>\n"
				    "    <mimetype>x-scheme-handler/http</mimetype>\n"
				    "    <mimetype>x-scheme-handler/https</mimetype>\n"
				    "  </mimetypes>\n"
				    "</component>\n";

	metad = as_metadata_new ();

	tmp = g_build_filename (datadir, "appdata.xml", NULL);
	file = g_file_new_for_path (tmp);
	g_free (tmp);

	as_metadata_set_locale (metad, "ALL");
	as_metadata_parse_file (metad, file, AS_FORMAT_KIND_XML, &error);
	cpt = as_metadata_get_component (metad);
	g_assert_no_error (error);
	g_assert (cpt != NULL);
	g_object_unref (file);

	as_component_sort_values (cpt);

	tmp = as_metadata_component_to_metainfo (metad,
						 AS_FORMAT_KIND_XML,
						 &error);
	g_assert_no_error (error);

	g_assert (as_test_compare_lines (tmp, EXPECTED_XML));
	g_free (tmp);

	g_object_unref (metad);
}

/**
 * as_xml_test_read_data:
 *
 * Helper function for other tests.
 */
static AsComponent*
as_xml_test_read_data (const gchar *data, AsFormatStyle mode)
{
	AsComponent *cpt = NULL;
	GPtrArray *cpts;
	g_autoptr(AsMetadata) metad = NULL;
	g_autoptr(GError) error = NULL;

	metad = as_metadata_new ();
	as_metadata_parse (metad, data, AS_FORMAT_KIND_XML, &error);
	g_assert_no_error (error);

	cpts = as_metadata_get_components (metad);
	g_assert_cmpint (cpts->len, >, 0);
	if (mode == AS_FORMAT_STYLE_METAINFO)
		g_assert_cmpint (cpts->len, ==, 1);
	cpt = AS_COMPONENT (g_ptr_array_index (cpts, 0));

	return g_object_ref (cpt);
}

/**
 * as_xml_test_serialize:
 *
 * Helper function for other tests.
 */
static gchar*
as_xml_test_serialize (AsComponent *cpt, AsFormatStyle mode)
{
	gchar *data;
	g_autoptr(AsMetadata) metad = NULL;
	g_autoptr(GError) error = NULL;

	metad = as_metadata_new ();
	as_metadata_add_component (metad, cpt);

	if (mode == AS_FORMAT_STYLE_METAINFO) {
		data = as_metadata_component_to_metainfo (metad, AS_FORMAT_KIND_XML, &error);
		g_assert_no_error (error);
	} else {
		data = as_metadata_components_to_collection (metad, AS_FORMAT_KIND_XML, &error);
		g_assert_no_error (error);
	}

	return data;
}

/**
 * test_appstream_write_description:
 *
 * Test writing the description tag for catalog and metainfo XML.
 */
static void
test_appstream_write_description ()
{
	guint i;
	gchar *tmp;
	AsRelease *rel;
	g_autoptr(AsMetadata) metad = NULL;
	g_autoptr(AsComponent) cpt = NULL;

	const gchar *EXPECTED_XML = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
				    "<component>\n"
				    "  <id>org.example.Test</id>\n"
				    "  <name>Test</name>\n"
				    "  <summary>Just a unittest.</summary>\n"
				    "  <description>\n"
				    "    <p>First paragraph</p>\n"
				    "    <ol>\n"
				    "      <li>One</li>\n"
				    "      <li>Two</li>\n"
				    "      <li>Three is &gt; 2 &amp; 1</li>\n"
				    "    </ol>\n"
				    "    <p>Paragraph2</p>\n"
				    "    <ul>\n"
				    "      <li>First</li>\n"
				    "      <li>Second</li>\n"
				    "    </ul>\n"
				    "    <p>Paragraph3 &amp; the last one</p>\n"
				    "  </description>\n"
				    "  <icon type=\"cached\" width=\"20\" height=\"20\">test_writetest.png</icon>\n"
				    "  <icon type=\"cached\" width=\"40\" height=\"40\">test_writetest.png</icon>\n"
				    "  <icon type=\"stock\">xml-writetest</icon>\n"
				    "  <releases>\n"
				    "    <release version=\"1.0\" date=\"2016-04-11T22:00:00Z\"/>\n"
				    "  </releases>\n"
				    "</component>\n";

	const gchar *EXPECTED_XML_LOCALIZED = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
						"<component>\n"
						"  <id>org.example.Test</id>\n"
						"  <name>Test</name>\n"
						"  <summary>Just a unittest.</summary>\n"
						"  <summary xml:lang=\"de\">Nur ein Unittest.</summary>\n"
						"  <description>\n"
						"    <p>First paragraph</p>\n"
						"    <ol>\n"
						"      <li>One</li>\n"
						"      <li>Two</li>\n"
						"      <li>Three is &gt; 2 &amp; 1</li>\n"
						"    </ol>\n"
						"    <p>Paragraph2</p>\n"
						"    <ul>\n"
						"      <li>First</li>\n"
						"      <li>Second</li>\n"
						"    </ul>\n"
						"    <p>Paragraph3 &amp; the last one</p>\n"
						"    <p xml:lang=\"de\">First paragraph</p>\n"
						"    <ol>\n"
						"      <li xml:lang=\"de\">One</li>\n"
						"      <li xml:lang=\"de\">Two</li>\n"
						"      <li xml:lang=\"de\">Three</li>\n"
						"    </ol>\n"
						"    <ul>\n"
						"      <li xml:lang=\"de\">First</li>\n"
						"      <li xml:lang=\"de\">Second</li>\n"
						"    </ul>\n"
						"    <p xml:lang=\"de\">Paragraph2</p>\n"
						"  </description>\n"
						"  <icon type=\"cached\" width=\"20\" height=\"20\">test_writetest.png</icon>\n"
						"  <icon type=\"cached\" width=\"40\" height=\"40\">test_writetest.png</icon>\n"
						"  <icon type=\"stock\">xml-writetest</icon>\n"
						"  <releases>\n"
						"    <release version=\"1.0\" date=\"2016-04-11T22:00:00Z\"/>\n"
						"  </releases>\n"
						"</component>\n";

	const gchar *EXPECTED_XML_DISTRO = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
					   "<components version=\"0.11\">\n"
					   "  <component>\n"
					   "    <id>org.example.Test</id>\n"
					   "    <name>Test</name>\n"
					   "    <summary>Just a unittest.</summary>\n"
					   "    <summary xml:lang=\"de\">Nur ein Unittest.</summary>\n"
					   "    <description>\n"
					   "      <p>First paragraph</p>\n"
					   "      <ol>\n"
					   "        <li>One</li>\n"
					   "        <li>Two</li>\n"
					   "        <li>Three is &gt; 2 &amp; 1</li>\n"
					   "      </ol>\n"
					   "      <p>Paragraph2</p>\n"
					   "      <ul>\n"
					   "        <li>First</li>\n"
					   "        <li>Second</li>\n"
					   "      </ul>\n"
					   "      <p>Paragraph3 &amp; the last one</p>\n"
					   "    </description>\n"
					   "    <description xml:lang=\"de\">\n"
					   "      <p>First paragraph</p>\n"
					   "      <ol>\n"
					   "        <li>One</li>\n"
					   "        <li>Two</li>\n"
					   "        <li>Three</li>\n"
					   "      </ol>\n"
					   "      <ul>\n"
					   "        <li>First</li>\n"
					   "        <li>Second</li>\n"
					   "      </ul>\n"
					   "      <p>Paragraph2</p>\n"
					   "    </description>\n"
					   "    <icon type=\"cached\" width=\"20\" height=\"20\">test_writetest.png</icon>\n"
					   "    <icon type=\"cached\" width=\"40\" height=\"40\">test_writetest.png</icon>\n"
					   "    <icon type=\"stock\">xml-writetest</icon>\n"
					   "    <releases>\n"
					   "      <release version=\"1.0\" timestamp=\"1460412000\"/>\n"
					   "    </releases>\n"
					   "  </component>\n"
					   "</components>\n";

	metad = as_metadata_new ();

	cpt = as_component_new ();
	as_component_set_kind (cpt, AS_COMPONENT_KIND_GENERIC);
	as_component_set_id (cpt, "org.example.Test");
	as_component_set_name (cpt, "Test", "C");
	as_component_set_summary (cpt, "Just a unittest.", "C");
	as_component_set_description (cpt,
				"<p>First paragraph</p>\n<ol><li>One</li><li>Two</li><li>Three is &gt; 2 &amp; 1</li></ol>\n<p>Paragraph2</p><ul><li>First</li><li>Second</li></ul><p>Paragraph3 &amp; the last one</p>",
				NULL);
	rel = as_release_new ();
	as_release_set_version (rel, "1.0");
	as_release_set_timestamp (rel, 1460412000);
	as_component_add_release (cpt, rel);
	g_object_unref (rel);

	for (i = 1; i <= 3; i++) {
		g_autoptr(AsIcon) icon = NULL;

		icon = as_icon_new ();
		if (i != 3)
			as_icon_set_kind (icon, AS_ICON_KIND_CACHED);
		else
			as_icon_set_kind (icon, AS_ICON_KIND_STOCK);
		as_icon_set_width (icon, i * 20);
		as_icon_set_height (icon, i * 20);

		if (i != 3)
			as_icon_set_filename (icon, "test_writetest.png");
		else
			as_icon_set_filename (icon, "xml-writetest");

		as_component_add_icon (cpt, icon);
	}

	as_metadata_add_component (metad, cpt);

	tmp = as_metadata_component_to_metainfo (metad, AS_FORMAT_KIND_XML, NULL);
	g_assert (as_test_compare_lines (tmp, EXPECTED_XML));
	g_free (tmp);

	/* add localization */
	as_component_set_summary (cpt, "Nur ein Unittest.", "de");
	as_component_set_description (cpt,
				"<p>First paragraph</p>\n<ol><li>One</li><li>Two</li><li>Three</li></ol><ul><li>First</li><li>Second</li></ul><p>Paragraph2</p>",
				"de");

	tmp = as_metadata_component_to_metainfo (metad, AS_FORMAT_KIND_XML, NULL);
	g_assert (as_test_compare_lines (tmp, EXPECTED_XML_LOCALIZED));
	g_free (tmp);

	tmp = as_metadata_components_to_collection (metad, AS_FORMAT_KIND_XML, NULL);
	g_assert (as_test_compare_lines (tmp, EXPECTED_XML_DISTRO));
	g_free (tmp);
}

/**
 * test_appstream_read_description:
 *
 * Test reading the description tag.
 */
static void
test_appstream_read_description (void)
{
	g_autoptr(AsComponent) cpt = NULL;
	const gchar *xmldata_desc_mi1 = "<component>\n"
					"  <id>org.example.DescTestMI-1</id>\n"
					"  <description>\n"
					"    <p>Agenda is a simple, slick, speedy and no-nonsense task manager. Use it to keep track of the tasks that matter most.</p>\n"
					"    <ul>\n"
					"      <li>Blazingly fast and light</li>\n"
					"      <li>Remembers your list until you clear completed tasks</li>\n"
					"      <li>...</li>\n"
					"    </ul>\n"
					"    <p>I dare you to find an easier, faster, more beautiful task manager for elementary OS.</p>\n"
					"  </description>\n"
					"</component>\n";

	const gchar *xmldata_desc_mi2 = "<component>\n"
					"  <id>org.example.DescTestMI-2</id>\n"
					"  <description>\n"
					"    <ul>\n"
					"      <li>I start with bullet points</li>\n"
					"      <li>Yes, this is allowed now</li>\n"
					"      <li>...</li>\n"
					"    </ul>\n"
					"    <p>Paragraph</p>\n"
					"  </description>\n"
					"</component>\n";

	cpt = as_xml_test_read_data (xmldata_desc_mi1, AS_FORMAT_STYLE_METAINFO);
	g_assert_cmpstr (as_component_get_id (cpt), ==, "org.example.DescTestMI-1");

	g_assert_cmpstr (as_component_get_description (cpt), ==, "<p>Agenda is a simple, slick, speedy and no-nonsense task manager. Use it to keep track of the tasks that matter most.</p>\n"
								 "<ul>\n"
								 "  <li>Blazingly fast and light</li>\n"
								 "  <li>Remembers your list until you clear completed tasks</li>\n"
								 "  <li>...</li>\n"
								 "</ul>\n"
								 "<p>I dare you to find an easier, faster, more beautiful task manager for elementary OS.</p>\n");

	g_object_unref (cpt);
	cpt = as_xml_test_read_data (xmldata_desc_mi2, AS_FORMAT_STYLE_METAINFO);
	g_assert_cmpstr (as_component_get_id (cpt), ==, "org.example.DescTestMI-2");

	g_assert_cmpstr (as_component_get_description (cpt), ==, "<ul>\n"
								 "  <li>I start with bullet points</li>\n"
								 "  <li>Yes, this is allowed now</li>\n"
								 "  <li>...</li>\n"
								 "</ul>\n"
								 "<p>Paragraph</p>\n");
}

/**
 * test_xml_read_url:
 *
 * Test reading the url tag.
 */
static void
test_xml_read_url (void)
{
	g_autoptr(AsComponent) cpt = NULL;
	const gchar *xmldata_languages = "<component>\n"
					 "  <id>org.example.UrlTest</id>\n"
					 "  <url type=\"homepage\">https://example.org</url>\n"
					 "  <url type=\"faq\">https://example.org/faq</url>\n"
					 "  <url type=\"donation\">https://example.org/donate</url>\n"
					 "</component>\n";

	cpt = as_xml_test_read_data (xmldata_languages, AS_FORMAT_STYLE_METAINFO);
	g_assert_cmpstr (as_component_get_id (cpt), ==, "org.example.UrlTest");

	g_assert_cmpstr (as_component_get_url (cpt, AS_URL_KIND_HOMEPAGE), ==, "https://example.org");
	g_assert_cmpstr (as_component_get_url (cpt, AS_URL_KIND_FAQ), ==, "https://example.org/faq");
	g_assert_cmpstr (as_component_get_url (cpt, AS_URL_KIND_DONATION), ==, "https://example.org/donate");
}

/**
 * test_xml_read_languages:
 *
 * Test reading the languages tag.
 */
static void
test_xml_read_languages (void)
{
	g_autoptr(AsComponent) cpt = NULL;
	const gchar *xmldata_languages = "<component>\n"
					 "  <id>org.example.LangTest</id>\n"
					 "  <languages>\n"
					 "    <lang percentage=\"48\">de_DE</lang>\n"
					 "    <lang percentage=\"100\">en_GB</lang>\n"
					 "  </languages>\n"
					 "</component>\n";

	cpt = as_xml_test_read_data (xmldata_languages, AS_FORMAT_STYLE_METAINFO);
	g_assert_cmpstr (as_component_get_id (cpt), ==, "org.example.LangTest");

	g_assert_cmpint (as_component_get_language (cpt, "de_DE"), ==, 48);
	g_assert_cmpint (as_component_get_language (cpt, "en_GB"), ==, 100);
	g_assert_cmpint (as_component_get_language (cpt, "invalid_C"), ==, -1);
}

/**
 * test_xml_write_languages:
 *
 * Test writing the languages tag.
 */
static void
test_xml_write_languages (void)
{
	g_autoptr(AsComponent) cpt = NULL;
	g_autofree gchar *res = NULL;
	const gchar *expected_lang_xml = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
					 "<component>\n"
					 "  <id>org.example.LangTest</id>\n"
					 "  <languages>\n"
					 "    <lang percentage=\"86\">de_DE</lang>\n"
					 "    <lang percentage=\"98\">en_GB</lang>\n"
					 "  </languages>\n"
					 "</component>\n";

	cpt = as_component_new ();
	as_component_set_id (cpt, "org.example.LangTest");
	as_component_add_language (cpt, "de_DE", 86);
	as_component_add_language (cpt, "en_GB", 98);

	res = as_xml_test_serialize (cpt, AS_FORMAT_STYLE_METAINFO);
	g_assert (as_test_compare_lines (res, expected_lang_xml));
}

/**
 * test_xml_write_releases:
 *
 * Test writing the releases tag.
 */
static void
test_xml_write_releases (void)
{
	g_autoptr(AsComponent) cpt = NULL;
	g_autoptr(AsRelease) rel = NULL;
	g_autofree gchar *res = NULL;
	const gchar *expected_rel_xml = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
					"<component>\n"
					"  <id>org.example.ReleaseTest</id>\n"
					"  <releases>\n"
					"    <release version=\"1.2\">\n"
					"      <description>\n"
					"        <p>A release description.</p>\n"
					"        <p xml:lang=\"de\">Eine Beschreibung der Veröffentlichung.</p>\n"
					"      </description>\n"
					"    </release>\n"
					"  </releases>\n"
					"</component>\n";

	cpt = as_component_new ();
	as_component_set_id (cpt, "org.example.ReleaseTest");

	rel = as_release_new ();
	as_release_set_version (rel, "1.2");
	as_release_set_description (rel, "<p>A release description.</p>", "C");
	as_release_set_description (rel, "<p>Eine Beschreibung der Veröffentlichung.</p>", "de");

	as_component_add_release (cpt, rel);

	res = as_xml_test_serialize (cpt, AS_FORMAT_STYLE_METAINFO);
	g_assert (as_test_compare_lines (res, expected_rel_xml));
}

/**
 * test_xml_write_provides:
 *
 * Test writing the provides tag.
 */
static void
test_xml_write_provides (void)
{
	g_autoptr(AsComponent) cpt = NULL;
	g_autoptr(AsProvided) prov_mime = NULL;
	g_autoptr(AsProvided) prov_bin = NULL;
	g_autoptr(AsProvided) prov_dbus = NULL;
	g_autofree gchar *res = NULL;
	const gchar *expected_prov_xml = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
					"<component>\n"
					"  <id>org.example.ProvidesTest</id>\n"
					"  <mimetypes>\n"
					"    <mimetype>text/plain</mimetype>\n"
					"    <mimetype>application/xml</mimetype>\n"
					"    <mimetype>image/png</mimetype>\n"
					"  </mimetypes>\n"
					"  <provides>\n"
					"    <binary>foobar</binary>\n"
					"    <binary>foobar-viewer</binary>\n"
					"    <dbus type=\"system\">org.example.ProvidesTest.Modify</dbus>\n"
					"  </provides>\n"
					"</component>\n";

	cpt = as_component_new ();
	as_component_set_id (cpt, "org.example.ProvidesTest");

	prov_mime = as_provided_new ();
	as_provided_set_kind (prov_mime, AS_PROVIDED_KIND_MIMETYPE);
	as_provided_add_item (prov_mime, "text/plain");
	as_provided_add_item (prov_mime, "application/xml");
	as_provided_add_item (prov_mime, "image/png");
	as_component_add_provided (cpt, prov_mime);

	prov_bin = as_provided_new ();
	as_provided_set_kind (prov_bin, AS_PROVIDED_KIND_BINARY);
	as_provided_add_item (prov_bin, "foobar");
	as_provided_add_item (prov_bin, "foobar-viewer");
	as_component_add_provided (cpt, prov_bin);

	prov_dbus = as_provided_new ();
	as_provided_set_kind (prov_dbus, AS_PROVIDED_KIND_DBUS_SYSTEM);
	as_provided_add_item (prov_dbus, "org.example.ProvidesTest.Modify");
	as_component_add_provided (cpt, prov_dbus);

	res = as_xml_test_serialize (cpt, AS_FORMAT_STYLE_METAINFO);
	g_assert (as_test_compare_lines (res, expected_prov_xml));
}

/**
 * test_xml_write_suggests:
 *
 * Test writing the suggests tag.
 */
static void
test_xml_write_suggests (void)
{
	g_autoptr(AsComponent) cpt = NULL;
	g_autoptr(AsSuggested) sug_us = NULL;
	g_autoptr(AsSuggested) sug_hr = NULL;
	g_autofree gchar *res = NULL;
	const gchar *expected_sug_xml_mi = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
					"<component>\n"
					"  <id>org.example.SuggestsTest</id>\n"
					"  <suggests type=\"upstream\">\n"
					"    <id>org.example.Awesome</id>\n"
					"  </suggests>\n"
					"</component>\n";
	const gchar *expected_sug_xml_coll = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
					"<components version=\"0.11\">\n"
					"  <component>\n"
					"    <id>org.example.SuggestsTest</id>\n"
					"    <suggests type=\"upstream\">\n"
					"      <id>org.example.Awesome</id>\n"
					"    </suggests>\n"
					"    <suggests type=\"heuristic\">\n"
					"      <id>org.example.MachineLearning</id>\n"
					"      <id>org.example.Stuff</id>\n"
					"    </suggests>\n"
					"  </component>\n"
					"</components>\n";

	cpt = as_component_new ();
	as_component_set_id (cpt, "org.example.SuggestsTest");

	sug_us = as_suggested_new ();
	as_suggested_set_kind (sug_us, AS_SUGGESTED_KIND_UPSTREAM);
	as_suggested_add_id (sug_us, "org.example.Awesome");
	as_component_add_suggested (cpt, sug_us);

	sug_hr = as_suggested_new ();
	as_suggested_set_kind (sug_hr, AS_SUGGESTED_KIND_HEURISTIC);
	as_suggested_add_id (sug_hr, "org.example.MachineLearning");
	as_suggested_add_id (sug_hr, "org.example.Stuff");
	as_component_add_suggested (cpt, sug_hr);

	/* test metainfo serialization */
	res = as_xml_test_serialize (cpt, AS_FORMAT_STYLE_METAINFO);
	g_assert (as_test_compare_lines (res, expected_sug_xml_mi));

	/* test collection serialization */
	res = as_xml_test_serialize (cpt, AS_FORMAT_STYLE_COLLECTION);
	g_assert (as_test_compare_lines (res, expected_sug_xml_coll));
}

static const gchar *xmldata_custom = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
					 "<component>\n"
					 "  <id>org.example.CustomTest</id>\n"
					 "  <custom>\n"
					 "    <value key=\"command\">myapp --go</value>\n"
					 "    <value key=\"foo\">bar</value>\n"
					 "  </custom>\n"
					 "</component>\n";
/**
 * test_xml_read_custom:
 *
 * Test reading the custom tag.
 */
static void
test_xml_read_custom (void)
{
	g_autoptr(AsComponent) cpt = NULL;

	cpt = as_xml_test_read_data (xmldata_custom, AS_FORMAT_STYLE_METAINFO);
	g_assert_cmpstr (as_component_get_id (cpt), ==, "org.example.CustomTest");

	g_assert_cmpstr (as_component_get_custom_value (cpt, "command"), ==, "myapp --go");
	g_assert_cmpstr (as_component_get_custom_value (cpt, "foo"), ==, "bar");
	g_assert_null (as_component_get_custom_value (cpt, NULL));
}

/**
 * test_xml_write_custom:
 *
 * Test writing the custom tag.
 */
static void
test_xml_write_custom (void)
{
	g_autoptr(AsComponent) cpt = NULL;
	g_autofree gchar *res = NULL;

	cpt = as_component_new ();
	as_component_set_id (cpt, "org.example.CustomTest");
	as_component_insert_custom_value (cpt, "command", "myapp");
	as_component_insert_custom_value (cpt, "command", "myapp --go");
	as_component_insert_custom_value (cpt, "foo", "bar");
	as_component_insert_custom_value (cpt, NULL, "dummy");

	res = as_xml_test_serialize (cpt, AS_FORMAT_STYLE_METAINFO);
	g_assert (as_test_compare_lines (res, xmldata_custom));
}

static const gchar *xmldata_content_rating = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
						"<component>\n"
						"  <id>org.example.ContentRatingTest</id>\n"
						"  <content_rating type=\"oars-1.0\">\n"
						"    <content_attribute id=\"drugs-alcohol\">moderate</content_attribute>\n"
						"    <content_attribute id=\"language-humor\">mild</content_attribute>\n"
						"  </content_rating>\n"
						"</component>\n";
/**
 * test_xml_read_content_rating:
 *
 * Test reading the content_rating tag.
 */
static void
test_xml_read_content_rating (void)
{
	g_autoptr(AsComponent) cpt = NULL;
	AsContentRating *rating;

	cpt = as_xml_test_read_data (xmldata_content_rating, AS_FORMAT_STYLE_METAINFO);
	g_assert_cmpstr (as_component_get_id (cpt), ==, "org.example.ContentRatingTest");

	rating = as_component_get_content_rating (cpt, "oars-1.0");
	g_assert_nonnull (rating);

	g_assert_cmpint (as_content_rating_get_value (rating, "drugs-alcohol"), ==, AS_CONTENT_RATING_VALUE_MODERATE);
	g_assert_cmpint (as_content_rating_get_value (rating, "language-humor"), ==, AS_CONTENT_RATING_VALUE_MILD);
	g_assert_cmpint (as_content_rating_get_minimum_age (rating), ==, 13);
}

/**
 * test_xml_write_content_rating:
 *
 * Test writing the content_rating tag.
 */
static void
test_xml_write_content_rating (void)
{
	g_autoptr(AsComponent) cpt = NULL;
	g_autoptr(AsContentRating) rating = NULL;
	g_autofree gchar *res = NULL;

	cpt = as_component_new ();
	as_component_set_id (cpt, "org.example.ContentRatingTest");

	rating = as_content_rating_new ();
	as_content_rating_set_kind (rating, "oars-1.0");

	as_content_rating_set_value (rating, "drugs-alcohol", AS_CONTENT_RATING_VALUE_MODERATE);
	as_content_rating_set_value (rating, "language-humor", AS_CONTENT_RATING_VALUE_MILD);

	as_component_add_content_rating (cpt, rating);

	res = as_xml_test_serialize (cpt, AS_FORMAT_STYLE_METAINFO);
	g_assert (as_test_compare_lines (res, xmldata_content_rating));
}

static const gchar *xmldata_launch = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
					"<component>\n"
					"  <id>org.example.LaunchTest</id>\n"
					"  <launchable type=\"desktop-id\">org.example.Test.desktop</launchable>\n"
					"  <launchable type=\"desktop-id\">kde4-kool.desktop</launchable>\n"
					"</component>\n";
/**
 * test_xml_read_launch:
 *
 * Test reading the launch tag.
 */
static void
test_xml_read_launch (void)
{
	g_autoptr(AsComponent) cpt = NULL;
	AsLaunchable *launch;

	cpt = as_xml_test_read_data (xmldata_launch, AS_FORMAT_STYLE_METAINFO);
	g_assert_cmpstr (as_component_get_id (cpt), ==, "org.example.LaunchTest");

	launch = as_component_get_launchable (cpt, AS_LAUNCHABLE_KIND_DESKTOP_ID);
	g_assert_nonnull (launch);

	g_assert_cmpint (as_launchable_get_entries (launch)->len, ==, 2);
	g_assert_cmpstr (g_ptr_array_index (as_launchable_get_entries (launch), 0), ==, "org.example.Test.desktop");
	g_assert_cmpstr (g_ptr_array_index (as_launchable_get_entries (launch), 1), ==, "kde4-kool.desktop");
}

/**
 * test_xml_write_launch:
 *
 * Test writing the launch tag.
 */
static void
test_xml_write_launch (void)
{
	g_autoptr(AsComponent) cpt = NULL;
	g_autoptr(AsLaunchable) launch = NULL;
	g_autofree gchar *res = NULL;

	cpt = as_component_new ();
	as_component_set_id (cpt, "org.example.LaunchTest");

	launch = as_launchable_new ();
	as_launchable_set_kind (launch, AS_LAUNCHABLE_KIND_DESKTOP_ID);

	as_launchable_add_entry (launch, "org.example.Test.desktop");
	as_launchable_add_entry (launch, "kde4-kool.desktop");

	as_component_add_launchable (cpt, launch);

	res = as_xml_test_serialize (cpt, AS_FORMAT_STYLE_METAINFO);
	g_assert (as_test_compare_lines (res, xmldata_launch));
}

/**
 * test_appstream_write_metainfo_to_collection:
 */
static void
test_appstream_write_metainfo_to_collection (void)
{
	gchar *tmp;
	g_autoptr(AsMetadata) metad = NULL;
	g_autoptr(GError) error = NULL;

	const gchar *METAINFO_XML = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
					"<component>\n"
					"  <id>org.example.Test</id>\n"
					"  <name>Test</name>\n"
					"  <name xml:lang=\"eo\">Testo</name>\n"
					"  <name xml:lang=\"x-test\">I am a cruft entry</name>\n"
					"  <name xml:lang=\"de\">Test</name>\n"
					"  <summary>Just a unittest.</summary>\n"
					"  <summary xml:lang=\"de\">Nur ein Unittest.</summary>\n"
					"  <description>\n"
					"    <p>First paragraph</p>\n"
					"    <ol>\n"
					"      <li>One</li>\n"
					"      <li>Two</li>\n"
					"      <li>Three is &gt; 2 &amp; 1</li>\n"
					"    </ol>\n"
					"    <p>Paragraph2</p>\n"
					"    <ul>\n"
					"      <li>First</li>\n"
					"      <li>Second</li>\n"
					"    </ul>\n"
					"    <p>Paragraph3 &amp; the last one</p>\n"
					"    <p xml:lang=\"de\">Erster Absatz</p>\n"
					"    <ol>\n"
					"      <li>One</li>\n"
					"      <li xml:lang=\"de\">Eins</li>\n"
					"      <li xml:lang=\"de\">Zwei</li>\n"
					"      <li xml:lang=\"de\">Drei</li>\n"
					"    </ol>\n"
					"    <ul>\n"
					"      <li xml:lang=\"de\">Erster</li>\n"
					"      <li xml:lang=\"de\">Zweiter</li>\n"
					"    </ul>\n"
					"    <p xml:lang=\"de\">Absatz2</p>\n"
					"  </description>\n"
					"  <icon type=\"cached\" width=\"20\" height=\"20\">test_writetest.png</icon>\n"
					"  <icon type=\"cached\" width=\"40\" height=\"40\">test_writetest.png</icon>\n"
					"  <icon type=\"stock\">xml-writetest</icon>\n"
					"  <releases>\n"
					"    <release version=\"1.0\" date=\"2016-04-11T22:00:00+00:00\"/>\n"
					"  </releases>\n"
					"</component>\n";

	const gchar *EXPECTED_XML_COLL = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
					   "<components version=\"0.11\">\n"
					   "  <component>\n"
					   "    <id>org.example.Test</id>\n"
					   "    <name xml:lang=\"de\">Test</name>\n"
					   "    <name>Test</name>\n"
					   "    <name xml:lang=\"eo\">Testo</name>\n"
					   "    <summary>Just a unittest.</summary>\n"
					   "    <summary xml:lang=\"de\">Nur ein Unittest.</summary>\n"
					   "    <description>\n"
					   "      <p>First paragraph</p>\n"
					   "      <ol>\n"
					   "        <li>One</li>\n"
					   "        <li>Two</li>\n"
					   "        <li>Three is &gt; 2 &amp; 1</li>\n"
					   "      </ol>\n"
					   "      <p>Paragraph2</p>\n"
					   "      <ul>\n"
					   "        <li>First</li>\n"
					   "        <li>Second</li>\n"
					   "      </ul>\n"
					   "      <p>Paragraph3 &amp; the last one</p>\n"
					   "      <ol>\n"
					   "        <li>One</li>\n"
					   "      </ol>\n"
					   "      <ul/>\n"
					   "    </description>\n"
					   "    <description xml:lang=\"de\">\n"
					   "      <p>Erster Absatz</p>\n"
					   "      <ol>\n"
					   "        <li>Eins</li>\n"
					   "        <li>Zwei</li>\n"
					   "        <li>Drei</li>\n"
					   "      </ol>\n"
					   "      <ul>\n"
					   "        <li>Erster</li>\n"
					   "        <li>Zweiter</li>\n"
					   "      </ul>\n"
					   "      <p>Absatz2</p>\n"
					   "    </description>\n"
					   "    <icon type=\"cached\" width=\"20\" height=\"20\">test_writetest.png</icon>\n"
					   "    <icon type=\"cached\" width=\"40\" height=\"40\">test_writetest.png</icon>\n"
					   "    <icon type=\"stock\">xml-writetest</icon>\n"
					   "    <releases>\n"
					   "      <release version=\"1.0\" timestamp=\"1460332800\"/>\n"
					   "    </releases>\n"
					   "  </component>\n"
					   "</components>\n";

	metad = as_metadata_new ();
	as_metadata_set_locale (metad, "ALL");

	as_metadata_parse (metad, METAINFO_XML, AS_FORMAT_KIND_XML, &error);
	g_assert_no_error (error);

	as_metadata_set_format_style (metad, AS_FORMAT_STYLE_COLLECTION);

	tmp = as_metadata_components_to_collection (metad, AS_FORMAT_KIND_XML, NULL);
	g_assert (as_test_compare_lines (tmp, EXPECTED_XML_COLL));
	g_free (tmp);
}

/**
 * main:
 */
int
main (int argc, char **argv)
{
	int ret;

	if (argc == 0) {
		g_error ("No test directory specified!");
		return 1;
	}

	datadir = argv[1];
	g_assert (datadir != NULL);
	datadir = g_build_filename (datadir, "samples", NULL);
	g_assert (g_file_test (datadir, G_FILE_TEST_EXISTS) != FALSE);

	g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);
	g_test_init (&argc, &argv, NULL);

	/* only critical and error are fatal */
	g_log_set_fatal_mask (NULL, G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);

	g_test_add_func ("/XML/Screenshots", test_screenshot_handling);
	g_test_add_func ("/XML/LegacyData", test_appstream_parser_legacy);
	g_test_add_func ("/XML/Read/ParserLocale", test_appstream_parser_locale);
	g_test_add_func ("/XML/Write/WriterLocale", test_appstream_write_locale);

	g_test_add_func ("/XML/Read/Description", test_appstream_read_description);
	g_test_add_func ("/XML/Write/Description", test_appstream_write_description);

	g_test_add_func ("/XML/Read/Url", test_xml_read_url);

	g_test_add_func ("/XML/Write/Releases", test_xml_write_releases);
	g_test_add_func ("/XML/Write/Provides", test_xml_write_provides);
	g_test_add_func ("/XML/Write/Suggests", test_xml_write_suggests);

	g_test_add_func ("/XML/Read/Languages", test_xml_read_languages);
	g_test_add_func ("/XML/Write/Languages", test_xml_write_languages);

	g_test_add_func ("/XML/Read/Custom", test_xml_read_custom);
	g_test_add_func ("/XML/Write/Custom", test_xml_write_custom);

	g_test_add_func ("/XML/Read/ContentRating", test_xml_read_content_rating);
	g_test_add_func ("/XML/Write/ContentRating", test_xml_write_content_rating);

	g_test_add_func ("/XML/Read/Launchable", test_xml_read_launch);
	g_test_add_func ("/XML/Write/Launchable", test_xml_write_launch);

	g_test_add_func ("/XML/Write/MetainfoToCollection", test_appstream_write_metainfo_to_collection);

	ret = g_test_run ();
	g_free (datadir);
	return ret;
}
