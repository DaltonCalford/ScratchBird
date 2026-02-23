Since 6.0.0.834 SQL schemas were intruduced in Firebird DBMS (commit: b8be591c0f9e1811f47fac792c8dd2f10c4cea28).
Following changes did appear since that snapshot:
    * every DB object name that must be shown by test output now has prefix if its schema, with default name `"PUBLIC"`;
    * the displayed object names become enclosed in double quotes, even if they are ascii-only and without inner spaces, e.g. `"ID"` etc;
    * there is no ability to remove PUBLIC schema (to suppress its output);
Because of that, many tests needed to be re-implemented in order to make them pass on all checked FB (3.x ... 6.x and future versions).

In order to reduce volume of this job and to avoid separation of expected values (depending on whether major version is 6.x or prior),
every such test has to be changed as follows:
    * initiate 'substitutions' variable with list of tuples that were needed before (or make it empty, but anyway it must be created),
      e.g.:
          ```
          substitutions = [('[ \t]+', ' '), ('(-)?At trigger\\s+\\S+', 'At trigger')]
          ```
          or
          ```
          substitutions = []
          ```
    * append to this list pre-defined values from EXTERNAL file `act.files_dir/test_config.ini` as show below:
          ```
          addi_subst_settings = QA_GLOBALS['schema_n_quotes_suppress']
          addi_subst_tokens = addi_subst_settings['addi_subst']
          for p in addi_subst_tokens.split():
              substitutions.append( (p, '') )
          ```
    * NOTE.
      File `act.files_dir/test_config.ini` must exist in the `$QA_HOME/files/` directory.
      Content of this file is used in `$QA_HOME/firebird-qa/src/firebird/qa/plugin.py`, see QA_GLOBALS dictionary in its source.
      Among other sections, following must present in this file:
          ```
          [schema_n_quotes_suppress]
          addi_subst="PUBLIC". " '
          ```
      If another schema will appear, e.g. "SYSTEM", and we need to suppress its output then `addi_subst` must be changed like this:
          ```
          addi_subst="PUBLIC". "SYSTEM". " '
          ```

      Applying of tokens from 'addi_subst' parameter to the 'substitutions' will chage it to follwing:
          ```
          substitutions = [ ( <optional: previous tuples>, ('"PUBLIC".', ''), ('"', ''), ("'", '') ]
          ```
After this, every single/double quotes along with schema prefix(es) will be removed from DB object names in the test output.
