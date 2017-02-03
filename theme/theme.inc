<?php

/**
 * @file
 * Theme hooks.
 */

/**
 * Prepares variables for islandora_book_book templates.
 *
 * Default template: islandora-book-book.tpl.php.
 *
 * @param array $variables
 *   An associative array containing:
 *   - object: An AbstractObject for which to generate the display.
 */
function template_preprocess_islandora_book_book(array &$variables) {
  module_load_include('inc', 'islandora_paged_content', 'includes/utilities');
  module_load_include('inc', 'islandora', 'includes/solution_packs');
  module_load_include('inc', 'islandora', 'includes/metadata');
  module_load_include('inc', 'islandora', 'includes/datastream');
  drupal_add_js('misc/form.js');
  drupal_add_js('misc/collapse.js');

  $object = $variables['object'];

  $variables['viewer_id'] = islandora_get_viewer_id('islandora_book_viewers');
  $variables['viewer_params'] = array(
    'object' => $object,
    'pages' => islandora_paged_content_get_pages_custom($object),
    'page_progression' => islandora_paged_content_get_page_progression($object),
  );

  $variables['display_metadata'] = variable_get('islandora_book_metadata_display', FALSE);
  $variables['parent_collections'] = islandora_get_parents_from_rels_ext($object);
  $variables['metadata'] = islandora_retrieve_metadata_markup($object);
  $variables['description'] = islandora_retrieve_description_markup($object);
}

/**
 * Processes variables for islandora_book_book templates.
 *
 * Default template: islandora-book-book.tpl.php.
 *
 * @param array $variables
 *   An associative array containing:
 *   - object: An AbstractObject for which to generate the display.
 *   - viewer_id: The "ID" of the viewer to use.
 *   - viewer_params: An (associative) array of parameters to pass to the
 *     viewer.
 */
function template_process_islandora_book_book(array &$variables) {
  if ($variables['viewer_id'] && $variables['viewer_id'] != 'none') {
    $viewer_callback = islandora_get_viewer_callback($variables['viewer_id']);
    if (function_exists($viewer_callback)) {
      $viewer = $viewer_callback($variables['viewer_params'], $variables['object']);
      if ($viewer) {
        $variables['viewer'] = $viewer;
      }
    }
  }
}

/**
 * Prepares variables for islandora_book_page templates.
 *
 * Default template: islandora-book-page.tpl.php.
 *
 * @param array $variables
 *   An associative array containing:
 *   - object: An AbstractObject for which to generate the display.
 */
function template_preprocess_islandora_book_page(array &$variables) {
  module_load_include('inc', 'islandora', 'includes/solution_packs');
  $object = $variables['object'];
  $results = $object->relationships->get(FEDORA_RELS_EXT_URI, 'isMemberOf');
  $result = reset($results);
  $variables['book_object_id'] = $result ? $result['object']['value'] : FALSE;

  $params = array();

  foreach(explode("\n", $object['MODS']->content) as $row) {
      if(strpos($row, '<url') !== false) {
          $url = trim(str_replace(['<url type="local">', '</url>'], '', $row));
          break;
      }
  }

  if(!empty($url)) {
      $jp2_url = $url;
  } else if (isset($object['JP2']) && islandora_datastream_access(ISLANDORA_VIEW_OBJECTS, $object['JP2'])) {
    // Get token to allow access to XACML protected datastreams.
    // Always use token authentication in case there is a global policy.
    module_load_include('inc', 'islandora', 'includes/authtokens');
    $token = islandora_get_object_token($object->id, 'JP2', 2);
    $jp2_url = url("islandora/object/{$object->id}/datastream/JP2/view", array(
      'absolute' => TRUE,
      'query' => array('token' => $token),
    ));
  }

  $params['jp2_url'] = $jp2_url;

  $variables['viewer_id'] = islandora_get_viewer_id('islandora_book_page_viewers');
  $variables['viewer_params'] = $params;
}

/**
 * Processes variables for islandora_book_page templates.
 *
 * Default template: islandora-book-page.tpl.php.
 *
 * @param array $variables
 *   An associative array containing:
 *   - object: An AbstractObject for which to generate the display.
 *   - viewer_id: The "ID" of the viewer to use.
 *   - viewer_params: An (associative) array of parameters to pass to the
 *     viewer.
 */
function template_process_islandora_book_page(array &$variables) {
  if ($variables['viewer_id'] && $variables['viewer_id'] != 'none') {
    $viewer_callback = islandora_get_viewer_callback($variables['viewer_id']);
    if (function_exists($viewer_callback)) {
      $viewer = $viewer_callback($variables['viewer_params'], $variables['object']);
      if ($viewer) {
        $variables['viewer'] = $viewer;
      }
    }
  }
}

function islandora_paged_content_get_pages_custom(AbstractObject $object) {
  $pages = array();
  $qp = new islandoraSolrQueryProcessor();
  $qp->buildQuery(format_string('@field:"@pid"', array(
      '@field' => variable_get('islandora_solr_member_of_field', 'RELS_EXT_isMemberOf_uri_ms'),
      '@pid' => "info:fedora/{$object->id}")
  ));
  $label_field = variable_get('islandora_solr_object_label_field', 'fgs_label_s');
  $sequence_field = variable_get('islandora_paged_content_sequence_number_field', 'RELS_EXT_isSequenceNumber_literal_ms');
  $height_field = variable_get('islandora_paged_content_solr_height_field', 'RELS_INT_height_literal_s');
  $width_field = variable_get('islandora_paged_content_solr_width_field', 'RELS_INT_width_literal_s');
  $url_field = 'mods_url_local_ms';
  $qp->solrParams['fl'] = implode(', ', array(
    'PID',
    $label_field,
    $sequence_field,
    $height_field,
    $width_field,
    $url_field,
  ));
  $qp->solrStart = 0;
  $qp->solrLimit = 100000;
  $qp->executeQuery(FALSE);
  if ($qp->islandoraSolrResult['response']['numFound'] > 0) {
    foreach ($qp->islandoraSolrResult['response']['objects'] as $page) {
      $pages[$page['PID']] = array(
        'pid' => $page['PID'],
        'page' => (string) reset($page['solr_doc'][$sequence_field]),
        'label' => $page['object_label'],
      );
      if (isset($page['solr_doc'][$width_field])) {
        $pages[$page['PID']]['width'] = $page['solr_doc'][$width_field];
      }
      if (isset($page['solr_doc'][$height_field])) {
        $pages[$page['PID']]['height'] = $page['solr_doc'][$height_field];
      }
      if (isset($page['solr_doc'][$url_field])) {
        $pages[$page['PID']]['remote_url'] = $page['solr_doc'][$url_field][0];
      }
    }
  }
  // Sort the pages into their proper order.
  $sort = function($a, $b) {
    $a = (is_array($a) && isset($a['page'])) ? $a['page'] : 0;
    $b = (is_array($b) && isset($b['page'])) ? $b['page'] : 0;
    if ($a == $b) {
      return 0;
    }
    return ($a < $b) ? -1 : 1;
  };
  uasort($pages, $sort);

  return $pages;
}
