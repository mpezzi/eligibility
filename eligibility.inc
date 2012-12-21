<?php

/**
 * @file
 * Provides internal functionality for eligibility.
 */


/**
 * Eligibility is early.
 */
define('ELIGIBILITY_IS_EARLY', 1);

/**
 * Eligibility is late.
 */
define('ELIGIBILITY_IS_LATE', 2);

/**
 * Eligibility in range.
 */
define('ELIGIBILITY_IS_ELIGIBLE', 3);

/**
 * Eligibility not in range.
 */
define('ELIGIBILITY_IS_NOT_ELIGIBLE', 4);


/**
 * Checks whether a user is allowed to proceed with the specified event.
 *
 * @param @name
 *   The name of the restriction.
 */
function eligibility_is_allowed($type, $name, $identifier = NULL) {
  if ( is_null($identifier) ) {
    $identifier = ip_address();
  }

  // Look for an event before the expiration time.
  $event = db_select('eligibility', 'e')
    ->fields('e', array())
    ->condition('type', $type)
    ->condition('name', $name)
    ->condition('identifier', $identifier)
    ->condition('expiration', REQUEST_TIME, '>')
    ->orderBy('expiration', 'DESC')
    ->range(0, 1)
    ->execute()
    ->fetchObject();

  // If an event was found, they're not eligibile.
  return ! (bool) $event;
}

/**
 * Registers an event for the for the current visitor to the eligibility control mechanisim.
 *
 * @param $type
 *   An restriction type.
 * @param @name
 *   The name of the restriction.
 */
function eligibility_register_event($type, $name, $identifier = NULL, $time = NULL) {
  if ( is_null($identifier) ) {
    $identifier = ip_address();
  }

  if ( is_null($time) ) {
    $time = REQUEST_TIME;
  }

  // Default expiration to time.
  $expiration = $time;

  // Invoke restriction callback to return appropriate expiration time.
  if ( $restriction = eligibility_get_restrictions($type) ) {
    $expiration = call_user_func($restriction['callback'], $name, $identifier, $time);
  }

  db_insert('eligibility')
    ->fields(array(
      'type' => $type,
      'name' => $name,
      'identifier' => $identifier,
      'timestamp' => $time,
      'expiration' => $expiration,
    ))
    ->execute();
}

/**
 * Makes the eligibility control mechanisim forget an event for the current visitor.
 *
* @param $type
 *   An restriction type.
 * @param @name
 *   The name of the restriction.
 */
function eligibility_clear_event($type, $name, $identifier = NULL) {
  if ( !isset($identifier) ) {
    $identifier = ip_address();
  }
  db_delete('eligibility')
    ->condition('type', $type)
    ->condition('event', $name)
    ->condition('identifier', $identifier)
    ->execute();
}

/**
 * Purge expired eligibility events.
 */
function eligibility_purge_events() {
  db_delete('eligibility')
    ->condition('expiration', REQUEST_TIME, '<')
    ->execute();
}

/**
 * Builds a list of available restrictions.
 *
 * @param $type
 *   An restriction type.
 * @return
 *   An array of all restrictions, or a single restriction if $type is set.
 */
function eligibility_get_restrictions($type = NULL) {
  $restrictions = &drupal_static(__FUNCTION__);

  if ( !isset($restrictions) ) {
    // Invoke hook_eligibility_restriction_info().
    $restrictions = module_invoke_all('eligibility_restriction_info');

    // Invoke hook_eligibility_restriction_info_alter().
    drupal_alter('eligibility_restriction_info_alter', $restrictions);
  }

  // Return a single restriction if $name is set.
  if ( !is_null($type) ) {
    return isset($restrictions[$type]) ? $restrictions[$type] : FALSE;
  }

  return $restrictions;
}


























/**
 * Load a restriction.
 *
 * @param $rid
 *   A Restriction ID.
 * @return
 *   A Restriction Object or FALSE
 */
function xeligibility_restriction_load($rid) {
  $restrictions = &drupal_static(__FUNCTION__, array());

  if ( !isset($restrictions[$rid]) ) {
    $restriction = db_select('eligibility_restrictions', 'r')
      ->fields('r')
      ->condition('r.rid', $rid)
      ->execute()
      ->fetchObject();

    switch ( $restriction->type ) {
      case 'series':
        $restriction->data = db_select('eligibility_restrictions_series', 'rs')
          ->fields('rs')
          ->condition('rs.rid', $rid)
          ->orderBy('timestamp')
          ->execute()
          ->fetchAll();

        break;
      case 'interval':
        $restriction->data = db_select('eligibility_restrictions_interval', 'ri')
          ->fields('ri')
          ->condition('ri.rid', $rid)
          ->execute()
          ->fetchObject();
    }

    if ( $restriction->tid && $term = taxonomy_term_load($restriction->tid) ) {
      $restriction->term = $term;
    }
    else {
      $restriction->term = FALSE;
    }

    $restrictions[$rid] = $restriction;
  }

  return $restrictions[$rid];
}

/**
 * Load multiple restrictions.
 *
 * @param $param
 *   An array of conditions to match against the database query.
 * @return
 *   An array of Restriction Objects.
 */
function xeligibility_restriction_load_multiple($param = array()) {
  $query = db_select('eligibility_restrictions', 'r')
    ->fields('r', array('rid'));

  foreach ( $param as $field => $value ) {
    $query->condition($field, $value);
  }

  $restrictions = array();
  $result = $query->execute();
  foreach ( $result as $restriction ) {
    $restrictions[] = eligibility_restriction_load($restriction->rid);
  }

  return $restrictions;
}

/**
 * Save a restriction.
 *
 * @param $restriction
 *   A Restriction Object.
 */
function xeligibility_restriction_save(&$restriction) {
  $restriction->is_new = empty($restriction->rid);

  if ( $restriction->is_new ) {
    $status = drupal_write_record('eligibility_restrictions', $restriction);
  }
  else {
    $status = drupal_write_record('eligibility_restrictions', $restriction, 'rid');
  }
}

/**
 * Create an interval restriction.
 *
 * @param $params
 *   An array of parameters.
 */
function xeligibility_restriction_create_interval($params = array()) {
  $params += array(
    'start' => time(),
    'end' => 0,
    'duration' => 60,
    'tid' => 0,
    'status' => 1,
  );

  $restriction = (object) array(
    'type' => 'interval',
    'tid' => $params['tid'],
    'status' => $params['status'],
  );

  eligibility_restriction_save($restriction);

  db_insert('eligibility_restrictions_interval')
    ->fields(array(
      'rid' => $restriction->rid,
      'start' => $params['start'],
      'end' => $params['end'],
      'length' => $params['duration'],
    ))
    ->execute();

  drupal_set_message(t('Successfully created interval restriction.'));
}

/**
 * Create a series restriction.
 *
 * @param $params
 *   An array of parameters.
 */
function xeligibility_restriction_create_series($params = array()) {
  $params += array(
    'number' => 10,
    'start' => time(),
    'duration' => 60,
    'tid' => 0,
    'status' => 1,
  );

  $restriction = (object) array(
    'type' => 'series',
    'tid' => $params['tid'],
    'status' => $params['status'],
  );

  eligibility_restriction_save($restriction);

  for ( $i = 1; $i <= $params['number']; $i++ ) {
    $series = (object) array(
      'rid' => $restriction->rid,
      'timestamp' => $params['start'] + $i * $params['duration'],
      'status' => 1,
    );

    drupal_write_record('eligibility_restrictions_series', $series);
  }

  drupal_set_message(t('Successfully created series restriction.'));
}

/**
 * Delete a restriction.
 *
 * @param $rid
 *   A Restriction ID.
 */
function xeligibility_restriction_delete($rid) {
  db_delete('eligibility_restrictions')
    ->condition('rid', $rid)
    ->execute();
  db_delete('eligibility_restrictions_interval')
    ->condition('rid', $rid)
    ->execute();
  db_delete('eligibility_restrictions_series')
    ->condition('rid', $rid)
    ->execute();

  drupal_set_message(t('Successfully deleted restriction.'));
}

/**
 * Clear all restrictions.
 */
function xeligibility_clear_all() {
  db_truncate('eligibility_events')->execute();
  db_truncate('eligibility_restrictions')->execute();
  db_truncate('eligibility_restrictions_interval')->execute();
  db_truncate('eligibility_restrictions_series')->execute();
  drupal_set_message(t('Cleared all eligibility restrictions.'));
}

/**
 * Returns the eligibility for an account based on any restrictions.
 *
 * @param $account
 *   (optional) A User Object.
 * @param $tid
 *   (optional) A Category ID.
 * @param $timestamp
 *   (optional) A timestamp to check against, defaults to time().
 * @param $last_timestamp
 *   (optional) The last timestamp used during eligibility.
 * @return
 *   An Eligibility Object.
 */
function xeligibility_get($params = array()) {
  $params += array(
    'uid' => $GLOBALS['user']->uid,
    'tid' => 0,
    'timestamp' => time(),
    'last_timestamp' => NULL,
  );

  $event = NULL;
  if ( is_null($params['last_timestamp']) ) {
    if ( $event = eligibility_event_load_last($params['uid'], $params['tid']) ) {
      $params['last_timestamp'] = $event->timestamp;
    }
    else {
      $params['last_timestamp'] = FALSE;
    }
  }

  $status = ELIGIBILITY_IS_ELIGIBLE;
  $restrictions = eligibility_restriction_load_multiple(array('status' => 1, 'tid' => $params['tid']));
  if ( count($restrictions) > 0 ) {
    foreach ( $restrictions as $restriction ) {
      $function = 'eligibility_restriction_' . $restriction->type;
      if ( $status == ELIGIBILITY_IS_ELIGIBLE && function_exists($function) ) {
        $eligibility = $function($restriction, $params['timestamp'], $params['last_timestamp']);
        $eligibility->tid = $restriction->tid;
        $eligibility->term = $restriction->term;
        $status = $eligibility->status;
      }
    }
  }
  else {
    $eligibility = (object) array(
      'status' => ELIGIBILITY_IS_ELIGIBLE,
      'tid' => $params['tid'],
      'timestamp' => $params['timestamp'],
      'last_timestamp' => $params['last_timestamp'],
      'start' => 0,
      'end' => 0,
    );
  }

  // Calculate the time left until eligibility.
  if ( $eligibility->status == ELIGIBILITY_IS_NOT_ELIGIBLE && isset($eligibility->period_end) ) {
    $eligibility->next = $eligibility->period_end - $eligibility->timestamp;
  }
  elseif ( $eligibility->status == ELIGIBILITY_IS_EARLY ) {
    $eligibility->next = $eligibility->start - $eligibility->timestamp;
  }
  elseif ( $eligibility->status == ELIGIBILITY_IS_LATE ) {
    $eligibility->next = -1;
  }
  else {
    $eligibility->next = 0;
  }

  // Provide the last eligibility event.
  $eligibility->event = $event;

  // Provide a human readable version of the state.
  $eligibility->human_status = eligibility_human_status($eligibility);

  // Debug the eligibility object.
  eligibility_debug($eligibility);

  return $eligibility;
}

function xeligibility_human_status($eligibility) {
  switch ( $eligibility->status ) {
    case ELIGIBILITY_IS_EARLY:
      return t('Eligibility begins @start', array('@start' => format_date($eligibility->start)));
    case ELIGIBILITY_IS_LATE:
      return t('Eligibility ended @end', array('@end' => format_date($eligibility->end)));
    case ELIGIBILITY_IS_NOT_ELIGIBLE:
      return t('Next Eligibility in @next', array('@next' => format_interval($eligibility->next)));
    case ELIGIBILITY_IS_ELIGIBLE:
      return t('Eligible');
  }
}

/**
 * Save an eligibility event.
 *
 * @param $uid
 *   A User ID.
 * @param $tid
 *   A Category ID.
 * @param $data
 *   Data to save with event.
 * @param $timestamp
 *   (optional) A timestamp, defaults to time().
 * @return
 *   An Eligibility Event Object.
 */
function xeligibility_event_save($uid, $tid, $data = NULL, $timestamp = NULL) {
  $event = (object) array(
    'uid' => $uid,
    'tid' => $tid,
    'timestamp' => is_null($timestamp) ? time() : $timestamp,
    'data' => $data,
  );

  $user = user_load($uid);
  if ( $tid != 0 ) {
    $term = taxonomy_term_load($tid);
    $term_name = $term->name;
  }
  else {
    $term_name = t('General');
  }

  watchdog('eligibility', 'Successfully marked off %name for %term eligibility.', array('%name' => $user->name, '%term' => $term_name), WATCHDOG_NOTICE, l(t('edit'), 'user/' . $user->uid . '/edit'));

  drupal_write_record('eligibility_events', $event);

  return $event;
}

/**
 * Returns that last timestamp.
 *
 * @param $uid
 *   A User ID.
 * @param $tid
 *   A Category ID.
 * @return
 *   An Eligibility Event Object.
 */
function xeligibility_event_load_last($uid, $tid) {
  $event = db_select('eligibility_events', 'e')
    ->fields('e')
    ->condition('uid', $uid)
    ->condition('tid', $tid)
    ->orderBy('timestamp', 'DESC')
    ->range(0, 1)
    ->execute()
    ->fetchObject();

  $event->data = unserialize($event->data);

  return $event;
}

/**
 * Returns eligibility information for interval based time checking.
 *
 * @param $restriction
 *   A Restriction Object.
 * @param $timestamp
 *   A timestamp to check against.
 * @param $last_timestamp
 *   The last timestamp used during eligibility.
 */
function xeligibility_restriction_interval($restriction, $timestamp, $last_timestamp) {
  $start = $restriction->data->start;
  $end = $restriction->data->end;
  $length = $restriction->data->length;

  if ( $timestamp < $start ) {
    return (object) array(
      'status' => ELIGIBILITY_IS_EARLY,
      'timestamp' => $timestamp,
      'start' => $start,
      'end' => $end,
    );
  }
  elseif ( $end != 0 && $timestamp >= $end ) {
    return (object) array(
      'status' => ELIGIBILITY_IS_LATE,
      'timestamp' => $timestamp,
      'start' => $start,
      'end' => $end,
    );
  }
  else {
    $num_intervals = ( $timestamp - $start ) / $length;

    // Intervals divides into timestamp evenly.
    if ( ( $timestamp - $start ) % $length == 0 ) {
      $period_start = $start + $num_intervals * $length;
      $period_end = $start + ( $num_intervals + 1 ) * $length;
    }
    // Intervals isn't a whole number calculate max and min values.
    else {
      $min_num_intervals = floor($num_intervals);
      $max_num_intervals = ceil($num_intervals);
      $period_start = $start + $min_num_intervals * $length;
      $period_end = $start + $max_num_intervals * $length;
    }

    if ( $timestamp >= $period_start && $timestamp < $period_end ) {
      if ( !$last_timestamp || $last_timestamp < $period_start ) {
        return (object) array(
          'status' => ELIGIBILITY_IS_ELIGIBLE,
          'timestamp' => $timestamp,
          'last_timestamp' => $last_timestamp,
          'start' => $start,
          'end' => $end,
          'period_start' => (int) $period_start,
          'period_end' => (int) $period_end,
        );
      }
    }
  }

  return (object) array(
    'status' => ELIGIBILITY_IS_NOT_ELIGIBLE,
    'timestamp' => $timestamp,
    'last_timestamp' => $last_timestamp,
    'start' => $start,
    'end' => $end,
    'period_start' => (int) $period_start,
    'period_end' => (int) $period_end,
  );
}

/**
 * Returns an eligibility object for series based time checking.
 *
 * @param $restriction
 *   A Restriction Object.
 * @param $timestamp
 *   A timestamp to check against.
 * @param $last_timestamp
 *   The last transaction performed.
 * @return
 *   An Eligibility Object.
 */
function xeligibility_restriction_series($restriction, $timestamp, $last_timestamp) {
  $s = count($restriction->data);
  $series = $restriction->data;
  $start = $series[0]->timestamp;
  $end = $series[$s - 1]->timestamp;

  if ( $timestamp < $start ) {
    return (object) array(
      'status' => ELIGIBILITY_IS_EARLY,
      'timestamp' => $timestamp,
      'last_timestamp' => $last_timestamp,
      'start' => $start,
      'end' => $end,
    );
  }
  elseif ( $timestamp >= $end ) {
    return (object) array(
      'status' => ELIGIBILITY_IS_LATE,
      'timestamp' => $timestamp,
      'last_timestamp' => $last_timestamp,
      'start' => $start,
      'end' => $end,
    );
  }
  else {
    for ( $i = 0; $i < $s - 1; $i++ ) {
      if ( $timestamp >= $series[$i]->timestamp && $timestamp < $series[$i + 1]->timestamp ) {
        $period_start = $series[$i]->timestamp;
        $period_end = $series[$i + 1]->timestamp;

        if ( !$last_timestamp || $last_timestamp < $period_start ) {
          return (object) array(
            'status' => ELIGIBILITY_IS_ELIGIBLE,
            'timestamp' => $timestamp,
            'last_timestamp' => $last_timestamp,
            'start' => $start,
            'end' => $end,
            'period_start' => $period_start,
            'period_end' => $period_end,
          );
        }
      }
    }
  }

  return (object) array(
    'status' => ELIGIBILITY_IS_NOT_ELIGIBLE,
    'timestamp' => $timestamp,
    'last_timestamp' => $last_timestamp,
    'start' => $start,
    'end' => $end,
    'period_start' => $period_start,
    'period_end' => $period_end,
  );
}

/**
 * Debug Eligibility.
 *
 * @param $eligibility
 *   An Eligibility Object.
 */
function xeligibility_debug($eligibility) {
  if ( user_access('debug eligibility') && eligibility_config('debug') && module_exists('devel') ) {
    dpm($eligibility);
  }
}

/**
 * Returns Eligibility configuration settings.
 */
function xeligibility_config($variable = NULL) {
  $config = array(
    'debug' => variable_get('eligibility_debug', FALSE),
  );

  if ( !is_null($variable) ) {
    return isset($config[$variable]) ? $config[$variable] : FALSE;
  }

  return $config;
}