<?php

/**
 * @file
 * Menu Callbacks for eligibility module.
 */


/**
 * Displays eligibility information for an individual user.
 *
 * @param $account
 *   A User Object.
 */
function eligibility_user_eligibility($form, &$form_state, $account) {
  $header = array(
    array('data' => t('ID')),
    array('data' => t('Type')),
    array('data' => t('Category')),
    array('data' => t('Status')),
  );

  $rows = array();
  $restrictions = eligibility_restriction_load_multiple(array('status' => 1));
  foreach ( $restrictions as $restriction ) {
    $eligibility = eligibility_get(array('uid' => $account->uid, 'tid' => $restriction->tid));

    $rows[] = array(
      $restriction->rid,
      $restriction->type,
      isset($restriction->term->name) ? $restriction->term->name : t('Uncategoried'),
      $eligibility->human_status
    );
  }

  $form['restrictions'] = array(
    '#theme' => 'table',
    '#header' => $header,
    '#rows' => $rows,
    '#empty' => t('No eligibility restrictions.'),
  );

  return $form;
}
