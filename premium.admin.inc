<?php
/**
 * @file
 * Admin pages for the Premium module.
 */

/**
 * Main admin overview page.
 */
function premium_admin_overview_page() {
  // Custom page title, since the menu name is very short.
  drupal_set_title(t('Premium content settings'));

  // Get the currently available levels.
  $levels = premium_level_load_all();

  // Help text.
  $output = "Premium content is content that is restricted to certain users of a web site. To create premium content: ". l('add a level', 'admin/content/premium/add') .", ". l('set permissions', 'admin/user/permissions') ." for your users, set a default level for each ". l('content type', 'admin/content/types') .", and create new content as usual.";

  // Generate table headers.
  $header = array(
    array('data' => t('name'), 'colspan' => 1),
    array('data' => t('actions'), 'colspan' => 2),
  );

  // Generate table rows.
  $rows = array();
  foreach ($levels as $level) {
    $rows[] = array(
      $level->name,
      l(t('edit'), 'admin/content/premium/' . $level->machine_name . '/edit'),
      l(t('delete'), 'admin/content/premium/' . $level->machine_name . '/delete'),
    );
  }

  if (!$rows) {
    $rows[] = array(array('data' => t('No premium content levels been defined.'), 'colspan' => '3'));
  }

  $output .= theme('table', $header, $rows);

  return $output;
}

/**
 * Build the premium level editing form.
 *
 * If an existing level is passed in, an edit form with both Save and Delete
 * buttons will be built. Otherwise, a blank 'add new record' form, without
 * the Delete button, will be built.
 *
 * @ingroup forms
 * @see premium_admin_level_edit_form_submit()
 */
function premium_admin_level_edit_form(&$form_state, $level = FALSE) {
  // If we're editing a level, set the title accordingly.
  if ($level) {
    drupal_set_title(t('Edit level “@name”', array('@name' => $level->name)));
  }
  // If not, provide some default values.
  else {
    $level = (object) array(
      'name' => '',
      'proctection_type' => 'all',
      'duration' => 2,
      'duration_unit' => 'W',
      'denied_message' => t('Full text available to premium subscribers only'),
      'denied_message_format' => FILTER_FORMAT_DEFAULT,
    );
  }

  $form = array();

  // If we're editing an existing level, we'll add a value field to the form
  // containing the level's unique ID.
  if (!empty($level->level_id)) {
    $form['level_id'] = array(
      '#type' => 'value',
      '#value' => $level->level_id,
    );
  }

  $form['name'] = array(
    '#type' => 'textfield',
    '#title' => t('Name'),
    '#default_value' => $level->name,
    '#required' => TRUE,
  );

  $form['machine_name'] = array(
    '#type' => 'textfield', 
    '#title' => t('Machine-readable name'),
    '#description' => t('A unique machine-readable name. Can only contain lowercase letters, numbers, and underscores.'),
    '#default_value' => $level->machine_name, 
    '#maxlength' => 255, 
    '#required' => TRUE,
  );

  $form['old_machine_name'] = array(
    '#type' => 'value', 
    '#value' => $level->machine_name, 
  );

  $form['proctection_type'] = array(
    '#type' => 'radios',
    '#title' => t('Protection type'),
    '#default_value' => $level->proctection_type,
    '#options' => array(
      'all' => t('Premium items are permanently restricted'),
      'archive' => t('Protect archives only: Items switch to premium status after a specified period'),
      'latest' => t('Protect latest content only: Items switch to non-premium status after a specified period'),
    ),
  );

  $form['duration'] = array(
    '#type' => 'select',
    '#title' => t('Protection duration'),
    '#default_value' => $level->duration,
    '#options' => array(1 => 1, 2 => 2, 3 => 3, 4 => 4, 5 => 5, 6 => 6, 7 => 7, 8 => 8, 9 => 9,
      10 => 10, 11 => 11, 12 => 12, 13 => 13, 14 => 14, 15 => 15),
  );

  $form['duration_unit'] = array(
    '#type' => 'select',
    '#title' => t('Duration unit'),
    '#default_value' => $level->duration_unit,
    '#options' => array(
      'D' => t('Days'),
      'W' => t('Weeks'),
      'M' => t('Months'),
      'Y' => t('Years'),
    ),
  );

  $form['denied_message_fieldset'] = array(
    '#type' => 'fieldset',
    '#title' => t('Access denied message'),
    '#tree' => FALSE,
  );

  $form['denied_message_fieldset']['denied_message'] = array(
    '#type' => 'textarea',
    '#default_value' => $level->denied_message,
    '#cols' => 60,
    '#rows' => 6,
    '#description' => t('When a visitor doesn\'t have access to an item at this level they will see this message instead of its full text'),
  );

  $form['denied_message_fieldset']['denied_message_format'] = filter_form($level->denied_message_form, 1, array('denied_message_format'));

  $form['buttons']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save premium level'),
  );

  if (!empty($level->level_id)) {
    $form['buttons']['delete'] = array(
      '#value' => l(t('Delete'), 'admin/content/premium/' . $level->machine_name . '/delete'),
    );
  }

  return $form;
}


/**
 * General validation handler for the premium level add/edit form.
 *
 * @ingroup formapi
 * @see premium_admin_level_edit_form()
 * @see premium_admin_level_edit_submit()
 */
function premium_admin_level_edit_form_validate($form_id, &$form_state) {
  $machine_name = trim($form_state['values']['machine_name']);

  // Validate machine name.
  if (empty($machine_name) || !preg_match('/^[a-z0-9_]+$/', $machine_name)) {
    form_set_error('machine_name', t('Machine name can only contain lowercase letters, numbers, and underscores.'));
  }
}

/**
 * General submit handler for the premium level add/edit form.
 *
 * @ingroup formapi
 * @see premium_admin_level_edit_form()
 * @see premium_admin_level_edit_form_validate()
 */
function premium_admin_level_edit_form_submit($form_id, &$form_state) {
  $level = (object) $form_state['values'];
  $change = premium_level_save($level);

  // If machine name changed, update the node tables.
  if ($change == SAVED_UPDATED && $level->machine_name != $level->old_machine_name) {
    db_query("
      UPDATE {premium_nodes}
      SET level_name = '%s'
      WHERE level_name = '%s'
    ", array(
      ':new' => $level->machine_name,
      ':old' => $level->old_machine_name
    ));
  }

  $form_state['redirect'] = 'admin/content/premium';
}

/**
 * Build the delete confirmation form.
 *
 * A simple wrapper around Drupal's core confirm_form() function. Adds a value
 * field to store the ID of the level being deleted.
 *
 * @ingroup forms
 * @see premium_admin_level_delete_form_submit()
 * @see confirm_form()
 */
function premium_admin_level_delete_form(&$form_state, $level) {
  $form = array();

  $form['level_id'] = array(
    '#type' => 'value',
    '#value' => $level->level_id,
  );

  return confirm_form($form,
    t('Are you sure you want to delete %title?', array('%title' => $level->name)),
    isset($_GET['destination']) ? $_GET['destination'] : 'admin/content/premium',
    t('This action cannot be undone.'),
    t('Delete')
  );
}

/**
 * General submit handler for the delete confirmation form.
 *
 * Core's confirm_form() function adds the 'confirm' value element we check
 * against to ensure the form was properly submitted. If it's there, delete
 * the record and redirect to the overview form.
 *
 * @ingroup formapi
 * @see scaffolding_example_form()
 */
function premium_admin_level_delete_form_submit($form, &$form_state) {
  if ($form_state['values']['confirm']) {
    premium_level_delete($form_state['values']['machine_name']);
    drupal_set_message(t('Premium level was deleted.'));
  }

  $form_state['redirect'] = 'admin/content/premium';
}

