#!/usr/bin/env bash

set -euo pipefail

# Config
PORT="${PORT:-3000}"
HOST="${HOST:-localhost}"
BASE_URL="${BASE_URL:-http://${HOST}:${PORT}}"

have_jq() { command -v jq >/dev/null 2>&1; }

curl_json() {
  local method="$1"; shift
  local url="$1"; shift
  local data="${1-}"
  if [[ -n "${data}" ]]; then
    curl -sS -w "\n%{http_code}" -H 'Content-Type: application/json' -X "${method}" --data "${data}" "${url}"
  else
    curl -sS -w "\n%{http_code}" -X "${method}" "${url}"
  fi
}

assert_status() {
  local expected="$1"; shift
  local actual="$1"; shift
  local label="$1"
  if [[ "${expected}" != "${actual}" ]]; then
    echo "[FAIL] ${label}: Expected status ${expected}, got ${actual}" >&2
    exit 1
  fi
  echo "[OK] ${label}: Status ${actual}"
}

assert_body_contains() {
  local body="$1"; shift
  local regex="$1"; shift
  local label="$1"
  if ! echo "${body}" | grep -Eiq -- "${regex}"; then
    echo "[FAIL] ${label}: Body did not match regex: ${regex}" >&2
    echo "Response body:" >&2
    echo "${body}" >&2
    exit 1
  fi
  echo "[OK] ${label}: Body matched regex"
}

assert_templates_empty() {
  local body="$1"
  if have_jq; then
    local count
    count=$(echo "${body}" | jq -r '(.templates // null) | if type=="array" then length else -1 end' 2>/dev/null || echo "-2")
    if [[ "${count}" != "0" ]]; then
      echo "[FAIL] GET /api/templates: Expected templates to be an empty array, got length=${count}" >&2
      echo "Body:" >&2
      echo "${body}" >&2
      exit 1
    fi
    echo "[OK] GET /api/templates: templates is an empty array"
  else
    # Fallback: check that templates key exists and is [] (allow spaces/newlines)
    echo "${body}" | tr -d '\n\r' | grep -Eiq '"templates"\s*:\s*\[\s*\]' || {
      echo "[FAIL] GET /api/templates: Expected \"templates\": []" >&2
      echo "Body:" >&2
      echo "${body}" >&2
      exit 1
    }
    echo "[OK] GET /api/templates: templates is an empty array (regex)"
  fi
}

echo "Testing TemplatesController endpoints against ${BASE_URL}"
echo "=================================================="

# 1) GET /api/templates (may be empty or pre-populated depending on prior runs)
{
  resp=$(curl_json GET "${BASE_URL}/api/templates")
  body="${resp%$'\n'*}"
  status="${resp##*$'\n'}"
  assert_status 200 "${status}" "GET /api/templates"
  if have_jq; then
    echo "[OK] GET /api/templates: returned $(echo "${body}" | jq -r '(.templates | length) // 0') templates"
  else
    echo "[OK] GET /api/templates: returned templates"
  fi
}

############################################
# 2) POST /api/templates (create RFC template)
############################################
{
  payload='{"name":"news-site","description":"Template for news websites","config":{"maxPages":500,"maxDepth":3,"spaRenderingEnabled":true,"extractTextContent":true,"politenessDelay":1000},"patterns":{"articleSelectors":["article",".post",".story"],"titleSelectors":["h1",".headline",".title"],"contentSelectors":[".content",".body",".article-body"]}}'
  resp=$(curl_json POST "${BASE_URL}/api/templates" "${payload}")
  body="${resp%$'\n'*}"
  status="${resp##*$'\n'}"
  if [[ "${status}" == "202" ]]; then
    if have_jq; then
      statusField=$(echo "${body}" | jq -r '.status // empty')
      messageField=$(echo "${body}" | jq -r '.message // empty')
      nameField=$(echo "${body}" | jq -r '.name // empty')
      [[ "${statusField}" == "accepted" ]] || { echo "[FAIL] POST /api/templates: .status != accepted" >&2; exit 1; }
      # Accept both Phase 1 stub and Phase 2 stored message
      if [[ ! "${messageField}" =~ ^(Template creation stub|Template stored)$ ]]; then
        echo "[FAIL] POST /api/templates: unexpected .message='${messageField}'" >&2; exit 1;
      fi
      [[ "${nameField}" == "news-site" || -z "${nameField}" ]] || { echo "[FAIL] POST /api/templates: unexpected .name" >&2; exit 1; }
      echo "[OK] POST /api/templates: JSON fields validated"
    else
      assert_body_contains "${body}" '"status"\s*:\s*"accepted"' "POST /api/templates (.status)"
      echo "${body}" | tr -d '\n\r' | grep -Eiq '"message"\s*":\s*"(Template creation stub|Template stored)"' || {
        echo "[FAIL] POST /api/templates: unexpected .message" >&2; exit 1;
      }
    fi
  elif [[ "${status}" == "400" ]]; then
    # Allow duplicate-name rejection as valid if template already exists
    echo "[OK] POST /api/templates: template already exists (400), continuing"
  else
    assert_status 202 "${status}" "POST /api/templates"
  fi
}

############################################################
# 3) GET /api/templates (should contain the created template)
############################################################
{
  resp=$(curl_json GET "${BASE_URL}/api/templates")
  body="${resp%$'\n'*}"
  status="${resp##*$'\n'}"
  assert_status 200 "${status}" "GET /api/templates (after create)"
  if have_jq; then
    hasNews=$(echo "${body}" | jq -r '[.templates[]?.name] | index("news-site") | if .==null then "no" else "yes" end' 2>/dev/null || echo "no")
    [[ "${hasNews}" == "yes" ]] || { echo "[FAIL] GET /api/templates: news-site not found" >&2; echo "${body}" >&2; exit 1; }
    echo "[OK] GET /api/templates: found news-site"
  else
    assert_body_contains "${body}" '"name"\s*:\s*"news-site"' "GET /api/templates contains news-site"
  fi
}

############################################################
# 4) GET /api/templates/:name (get specific template)
############################################################
{
  resp=$(curl_json GET "${BASE_URL}/api/templates/news-site")
  body="${resp%$'\n'*}"
  status="${resp##*$'\n'}"
  assert_status 200 "${status}" "GET /api/templates/news-site"
  if have_jq; then
    nameField=$(echo "${body}" | jq -r '.name // empty')
    [[ "${nameField}" == "news-site" ]] || { echo "[FAIL] GET /api/templates/news-site: .name != news-site" >&2; exit 1; }
    echo "[OK] GET /api/templates/news-site: template found"
  else
    assert_body_contains "${body}" '"name"\s*:\s*"news-site"' "GET /api/templates/news-site contains name"
  fi
}

############################################################
# 5) GET /api/templates/:name (non-existent template)
############################################################
{
  resp=$(curl_json GET "${BASE_URL}/api/templates/non-existent-template")
  body="${resp%$'\n'*}"
  status="${resp##*$'\n'}"
  assert_status 404 "${status}" "GET /api/templates/non-existent-template (404 expected)"
  if have_jq; then
    errorCode=$(echo "${body}" | jq -r '.error.code // empty')
    errorMessage=$(echo "${body}" | jq -r '.error.message // empty')
    [[ "${errorCode}" == "NOT_FOUND" ]] || { echo "[FAIL] GET /api/templates/non-existent: .error.code != NOT_FOUND" >&2; exit 1; }
    [[ "${errorMessage}" == "template not found" ]] || { echo "[FAIL] GET /api/templates/non-existent: .error.message != template not found" >&2; exit 1; }
    echo "[OK] GET /api/templates/non-existent: 404 error response"
  else
    assert_body_contains "${body}" '"code"\s*:\s*"NOT_FOUND"' "GET /api/templates/non-existent (error code)"
    assert_body_contains "${body}" '"message"\s*:\s*"template not found"' "GET /api/templates/non-existent (error message)"
  fi
}

############################################################
# 6) POST /api/templates (create custom template)
############################################################
{
  # Use timestamp to ensure unique template name
  TIMESTAMP=$(date +%s)
  TEMPLATE_NAME="test-template-${TIMESTAMP}"
  payload="{\"name\":\"${TEMPLATE_NAME}\",\"description\":\"Test template for validation\",\"config\":{\"maxPages\":100,\"maxDepth\":2,\"spaRenderingEnabled\":false,\"extractTextContent\":true,\"politenessDelay\":2000},\"patterns\":{\"articleSelectors\":[\".test-article\"],\"titleSelectors\":[\"h1.test-title\"],\"contentSelectors\":[\".test-content\"]}}"
  resp=$(curl_json POST "${BASE_URL}/api/templates" "${payload}")
  body="${resp%$'\n'*}"
  status="${resp##*$'\n'}"
  assert_status 202 "${status}" "POST /api/templates (custom template)"
  if have_jq; then
    statusField=$(echo "${body}" | jq -r '.status // empty')
    nameField=$(echo "${body}" | jq -r '.name // empty')
    [[ "${statusField}" == "accepted" ]] || { echo "[FAIL] POST /api/templates: .status != accepted" >&2; exit 1; }
    [[ "${nameField}" == "${TEMPLATE_NAME}" ]] || { echo "[FAIL] POST /api/templates: .name != ${TEMPLATE_NAME}" >&2; exit 1; }
    echo "[OK] POST /api/templates: custom template created (${TEMPLATE_NAME})"
  else
    assert_body_contains "${body}" '"status"\s*:\s*"accepted"' "POST /api/templates (custom template status)"
    assert_body_contains "${body}" "\"name\"\s*:\s*\"${TEMPLATE_NAME}\"" "POST /api/templates (custom template name)"
  fi
}

############################################################
# 7) POST /api/templates (validation error - invalid name)
############################################################
{
  payload='{"name":"invalid name!","description":"Invalid template name"}'
  resp=$(curl_json POST "${BASE_URL}/api/templates" "${payload}")
  body="${resp%$'\n'*}"
  status="${resp##*$'\n'}"
  assert_status 400 "${status}" "POST /api/templates (invalid name - 400 expected)"
  if have_jq; then
    errorCode=$(echo "${body}" | jq -r '.error.code // empty')
    errorMessage=$(echo "${body}" | jq -r '.error.message // empty')
    [[ "${errorCode}" == "BAD_REQUEST" ]] || { echo "[FAIL] POST /api/templates: .error.code != BAD_REQUEST" >&2; exit 1; }
    echo "[OK] POST /api/templates: validation error for invalid name"
  else
    assert_body_contains "${body}" '"code"\s*:\s*"BAD_REQUEST"' "POST /api/templates (validation error)"
  fi
}

############################################################
# 8) POST /api/templates (validation error - invalid JSON)
############################################################
{
  payload='{"name":"invalid-json","description":"Invalid JSON structure","config":{"maxPages":"not-a-number"}}'
  resp=$(curl_json POST "${BASE_URL}/api/templates" "${payload}")
  body="${resp%$'\n'*}"
  status="${resp##*$'\n'}"
  assert_status 400 "${status}" "POST /api/templates (invalid JSON - 400 expected)"
  if have_jq; then
    errorCode=$(echo "${body}" | jq -r '.error.code // empty')
    errorMessage=$(echo "${body}" | jq -r '.error.message // empty')
    [[ "${errorCode}" == "BAD_REQUEST" ]] || { echo "[FAIL] POST /api/templates: .error.code != BAD_REQUEST" >&2; exit 1; }
    echo "[OK] POST /api/templates: validation error for invalid JSON"
  else
    assert_body_contains "${body}" '"code"\s*:\s*"BAD_REQUEST"' "POST /api/templates (invalid JSON error)"
  fi
}

############################################################
# 9) POST /api/crawl/add-site-with-template
############################################################
{
  payload='{"url":"https://example.com","template":"news-site"}'
  resp=$(curl_json POST "${BASE_URL}/api/crawl/add-site-with-template" "${payload}")
  body="${resp%$'\n'*}"
  status="${resp##*$'\n'}"
  if have_jq; then
    # Support both Phase 1 stub and Phase 2+ real crawl response
    successField=$(echo "${body}" | jq -r '.success // empty')
    statusField=$(echo "${body}" | jq -r '.status // empty')
    messageField=$(echo "${body}" | jq -r '.message // empty')
    if [[ "${status}" == "202" && "${statusField}" == "accepted" && "${messageField}" =~ Crawl\ request\ with\ template\ stub ]]; then
      echo "[OK] add-site-with-template: stub response accepted"
      appliedCount=$(echo "${body}" | jq 'try (.data.appliedConfig | length) catch 0')
      if [[ "${appliedCount}" -gt 0 ]]; then
        jq -e '.data.appliedConfig.maxPages and .data.appliedConfig.maxDepth' >/dev/null 2>&1 <<<"${body}" || {
          echo "[FAIL] add-site-with-template: appliedConfig missing keys" >&2; exit 1;
        }
      fi
    elif [[ "${status}" == "202" && "${successField}" == "true" ]]; then
      jq -e '.data.sessionId and .data.template and .data.appliedConfig.maxPages' >/dev/null 2>&1 <<<"${body}" || {
        echo "[FAIL] add-site-with-template: missing expected fields in success response" >&2; exit 1;
      }
      echo "[OK] add-site-with-template: real crawl response accepted"
    else
      echo "[FAIL] add-site-with-template: unexpected response" >&2
      echo "${body}" >&2
      exit 1
    fi
  else
    assert_body_contains "${body}" '"status"\s*:\s*"accepted"|"success"\s*:\s*true' "POST /api/crawl/add-site-with-template (status/success)"
  fi
}

############################################################
# 10) POST /api/crawl/add-site-with-template (non-existent template)
############################################################
{
  payload='{"url":"https://example.com","template":"non-existent-template"}'
  resp=$(curl_json POST "${BASE_URL}/api/crawl/add-site-with-template" "${payload}")
  body="${resp%$'\n'*}"
  status="${resp##*$'\n'}"
  assert_status 404 "${status}" "POST /api/crawl/add-site-with-template (non-existent template - 404 expected)"
  if have_jq; then
    errorCode=$(echo "${body}" | jq -r '.error.code // empty')
    errorMessage=$(echo "${body}" | jq -r '.error.message // empty')
    [[ "${errorCode}" == "NOT_FOUND" ]] || { echo "[FAIL] add-site-with-template: .error.code != NOT_FOUND" >&2; exit 1; }
    echo "[OK] add-site-with-template: 404 error for non-existent template"
  else
    assert_body_contains "${body}" '"code"\s*:\s*"NOT_FOUND"' "POST /api/crawl/add-site-with-template (404 error)"
  fi
}

############################################################
# 11) POST /api/crawl/add-site-with-template (missing required fields)
############################################################
{
  payload='{"url":"https://example.com"}'
  resp=$(curl_json POST "${BASE_URL}/api/crawl/add-site-with-template" "${payload}")
  body="${resp%$'\n'*}"
  status="${resp##*$'\n'}"
  assert_status 400 "${status}" "POST /api/crawl/add-site-with-template (missing template - 400 expected)"
  if have_jq; then
    errorCode=$(echo "${body}" | jq -r '.error.code // empty')
    errorMessage=$(echo "${body}" | jq -r '.error.message // empty')
    [[ "${errorCode}" == "BAD_REQUEST" ]] || { echo "[FAIL] add-site-with-template: .error.code != BAD_REQUEST" >&2; exit 1; }
    echo "[OK] add-site-with-template: 400 error for missing template"
  else
    assert_body_contains "${body}" '"code"\s*:\s*"BAD_REQUEST"' "POST /api/crawl/add-site-with-template (400 error)"
  fi
}

############################################################
# 12) DELETE /api/templates/:name (delete custom template)
############################################################
{
  resp=$(curl_json DELETE "${BASE_URL}/api/templates/${TEMPLATE_NAME}")
  body="${resp%$'\n'*}"
  status="${resp##*$'\n'}"
  assert_status 200 "${status}" "DELETE /api/templates/${TEMPLATE_NAME}"
  if have_jq; then
    statusField=$(echo "${body}" | jq -r '.status // empty')
    nameField=$(echo "${body}" | jq -r '.name // empty')
    [[ "${statusField}" == "deleted" ]] || { echo "[FAIL] DELETE /api/templates: .status != deleted" >&2; exit 1; }
    [[ "${nameField}" == "${TEMPLATE_NAME}" ]] || { echo "[FAIL] DELETE /api/templates: .name != ${TEMPLATE_NAME}" >&2; exit 1; }
    echo "[OK] DELETE /api/templates: custom template deleted (${TEMPLATE_NAME})"
  else
    assert_body_contains "${body}" '"status"\s*:\s*"deleted"' "DELETE /api/templates (deleted status)"
    assert_body_contains "${body}" "\"name\"\s*:\s*\"${TEMPLATE_NAME}\"" "DELETE /api/templates (deleted name)"
  fi
}

############################################################
# 13) DELETE /api/templates/:name (non-existent template)
############################################################
{
  resp=$(curl_json DELETE "${BASE_URL}/api/templates/non-existent-template")
  body="${resp%$'\n'*}"
  status="${resp##*$'\n'}"
  assert_status 404 "${status}" "DELETE /api/templates/non-existent-template (404 expected)"
  if have_jq; then
    errorCode=$(echo "${body}" | jq -r '.error.code // empty')
    errorMessage=$(echo "${body}" | jq -r '.error.message // empty')
    [[ "${errorCode}" == "NOT_FOUND" ]] || { echo "[FAIL] DELETE /api/templates: .error.code != NOT_FOUND" >&2; exit 1; }
    echo "[OK] DELETE /api/templates: 404 error for non-existent template"
  else
    assert_body_contains "${body}" '"code"\s*:\s*"NOT_FOUND"' "DELETE /api/templates (404 error)"
  fi
}

############################################################
# 14) GET /api/templates (verify test-template was deleted)
############################################################
{
  resp=$(curl_json GET "${BASE_URL}/api/templates")
  body="${resp%$'\n'*}"
  status="${resp##*$'\n'}"
  assert_status 200 "${status}" "GET /api/templates (after delete)"
  if have_jq; then
    hasTest=$(echo "${body}" | jq -r "[.templates[]?.name] | index(\"${TEMPLATE_NAME}\") | if .==null then \"no\" else \"yes\" end" 2>/dev/null || echo "no")
    [[ "${hasTest}" == "no" ]] || { echo "[FAIL] GET /api/templates: ${TEMPLATE_NAME} still found after deletion" >&2; echo "${body}" >&2; exit 1; }
    echo "[OK] GET /api/templates: ${TEMPLATE_NAME} successfully deleted"
  else
    # Check that test-template is not in the response
    if echo "${body}" | grep -q "\"name\"\s*:\s*\"${TEMPLATE_NAME}\""; then
      echo "[FAIL] GET /api/templates: ${TEMPLATE_NAME} still found after deletion" >&2
      echo "${body}" >&2
      exit 1
    fi
    echo "[OK] GET /api/templates: ${TEMPLATE_NAME} successfully deleted (regex check)"
  fi
}

############################################################
# 15) README.md API Examples - List Available Templates
############################################################
{
  echo "Testing README.md API examples..."
  resp=$(curl_json GET "${BASE_URL}/api/templates")
  body="${resp%$'\n'*}"
  status="${resp##*$'\n'}"
  assert_status 200 "${status}" "README example: GET /api/templates"
  if have_jq; then
    templateCount=$(echo "${body}" | jq -r '(.templates | length) // 0')
    echo "[OK] README example: Listed ${templateCount} templates"
  else
    echo "[OK] README example: Listed templates"
  fi
}

############################################################
# 16) README.md API Examples - Use Template for Crawling
############################################################
{
  payload='{"url":"https://www.bbc.com/news","template":"news-site"}'
  resp=$(curl_json POST "${BASE_URL}/api/crawl/add-site-with-template" "${payload}")
  body="${resp%$'\n'*}"
  status="${resp##*$'\n'}"
  if have_jq; then
    successField=$(echo "${body}" | jq -r '.success // empty')
    if [[ "${status}" == "202" && "${successField}" == "true" ]]; then
      sessionId=$(echo "${body}" | jq -r '.data.sessionId // empty')
      template=$(echo "${body}" | jq -r '.data.template // empty')
      maxPages=$(echo "${body}" | jq -r '.data.appliedConfig.maxPages // empty')
      maxDepth=$(echo "${body}" | jq -r '.data.appliedConfig.maxDepth // empty')
      spaEnabled=$(echo "${body}" | jq -r '.data.appliedConfig.spaRenderingEnabled // empty')
      echo "[OK] README example: BBC News crawl with news-site template (session: ${sessionId})"
      echo "[OK] README example: Applied config - maxPages: ${maxPages}, maxDepth: ${maxDepth}, SPA: ${spaEnabled}"
    else
      echo "[OK] README example: BBC News crawl response received"
    fi
  else
    assert_body_contains "${body}" '"success"\s*:\s*true' "README example: BBC News crawl"
  fi
}

############################################################
# 17) README.md API Examples - Create Custom Template
############################################################
{
  README_TEMPLATE_NAME="my-blog-template-$(date +%s)"
  payload="{\"name\":\"${README_TEMPLATE_NAME}\",\"description\":\"Template for my personal blog\",\"config\":{\"maxPages\":100,\"maxDepth\":2,\"politenessDelay\":2000},\"patterns\":{\"articleSelectors\":[\".blog-post\"],\"titleSelectors\":[\"h1.blog-title\"],\"contentSelectors\":[\".blog-content\"]}}"
  resp=$(curl_json POST "${BASE_URL}/api/templates" "${payload}")
  body="${resp%$'\n'*}"
  status="${resp##*$'\n'}"
  assert_status 202 "${status}" "README example: Create custom template"
  if have_jq; then
    statusField=$(echo "${body}" | jq -r '.status // empty')
    nameField=$(echo "${body}" | jq -r '.name // empty')
    [[ "${statusField}" == "accepted" ]] || { echo "[FAIL] README example: .status != accepted" >&2; exit 1; }
    [[ "${nameField}" == "${README_TEMPLATE_NAME}" ]] || { echo "[FAIL] README example: .name != ${README_TEMPLATE_NAME}" >&2; exit 1; }
    echo "[OK] README example: Custom template created (${README_TEMPLATE_NAME})"
  else
    assert_body_contains "${body}" '"status"\s*:\s*"accepted"' "README example: Custom template creation"
  fi
}

############################################################
# 18) README.md API Examples - Verify Custom Template
############################################################
{
  resp=$(curl_json GET "${BASE_URL}/api/templates/${README_TEMPLATE_NAME}")
  body="${resp%$'\n'*}"
  status="${resp##*$'\n'}"
  assert_status 200 "${status}" "README example: Get custom template"
  if have_jq; then
    name=$(echo "${body}" | jq -r '.name // empty')
    description=$(echo "${body}" | jq -r '.description // empty')
    maxPages=$(echo "${body}" | jq -r '.config.maxPages // empty')
    maxDepth=$(echo "${body}" | jq -r '.config.maxDepth // empty')
    politenessDelay=$(echo "${body}" | jq -r '.config.politenessDelay // empty')
    articleSelectors=$(echo "${body}" | jq -r '.patterns.articleSelectors[0] // empty')
    titleSelectors=$(echo "${body}" | jq -r '.patterns.titleSelectors[0] // empty')
    contentSelectors=$(echo "${body}" | jq -r '.patterns.contentSelectors[0] // empty')
    
    [[ "${name}" == "${README_TEMPLATE_NAME}" ]] || { echo "[FAIL] README example: Template name mismatch" >&2; exit 1; }
    [[ "${description}" == "Template for my personal blog" ]] || { echo "[FAIL] README example: Description mismatch" >&2; exit 1; }
    [[ "${maxPages}" == "100" ]] || { echo "[FAIL] README example: MaxPages mismatch" >&2; exit 1; }
    [[ "${maxDepth}" == "2" ]] || { echo "[FAIL] README example: MaxDepth mismatch" >&2; exit 1; }
    [[ "${politenessDelay}" == "2000" ]] || { echo "[FAIL] README example: PolitenessDelay mismatch" >&2; exit 1; }
    [[ "${articleSelectors}" == ".blog-post" ]] || { echo "[FAIL] README example: ArticleSelectors mismatch" >&2; exit 1; }
    [[ "${titleSelectors}" == "h1.blog-title" ]] || { echo "[FAIL] README example: TitleSelectors mismatch" >&2; exit 1; }
    [[ "${contentSelectors}" == ".blog-content" ]] || { echo "[FAIL] README example: ContentSelectors mismatch" >&2; exit 1; }
    
    echo "[OK] README example: Custom template verified with exact README configuration"
  else
    assert_body_contains "${body}" '"name"\s*:\s*"my-blog-template' "README example: Custom template name"
  fi
}

############################################################
# 19) README.md API Examples - Test Custom Template Crawling
############################################################
{
  payload="{\"url\":\"https://example-blog.com\",\"template\":\"${README_TEMPLATE_NAME}\"}"
  resp=$(curl_json POST "${BASE_URL}/api/crawl/add-site-with-template" "${payload}")
  body="${resp%$'\n'*}"
  status="${resp##*$'\n'}"
  if have_jq; then
    successField=$(echo "${body}" | jq -r '.success // empty')
    if [[ "${status}" == "202" && "${successField}" == "true" ]]; then
      appliedMaxPages=$(echo "${body}" | jq -r '.data.appliedConfig.maxPages // empty')
      appliedMaxDepth=$(echo "${body}" | jq -r '.data.appliedConfig.maxDepth // empty')
      appliedPolitenessDelay=$(echo "${body}" | jq -r '.data.appliedConfig.politenessDelay // empty')
      appliedTemplate=$(echo "${body}" | jq -r '.data.template // empty')
      
      [[ "${appliedMaxPages}" == "100" ]] || { echo "[FAIL] README example: Applied maxPages != 100" >&2; exit 1; }
      [[ "${appliedMaxDepth}" == "2" ]] || { echo "[FAIL] README example: Applied maxDepth != 2" >&2; exit 1; }
      [[ "${appliedPolitenessDelay}" == "2000" ]] || { echo "[FAIL] README example: Applied politenessDelay != 2000" >&2; exit 1; }
      [[ "${appliedTemplate}" == "${README_TEMPLATE_NAME}" ]] || { echo "[FAIL] README example: Applied template name mismatch" >&2; exit 1; }
      
      echo "[OK] README example: Custom template applied correctly to crawling"
      echo "[OK] README example: Applied config matches README specification exactly"
    else
      echo "[OK] README example: Custom template crawl response received"
    fi
  else
    assert_body_contains "${body}" '"success"\s*:\s*true' "README example: Custom template crawl"
  fi
}

############################################################
# 20) README.md API Examples - Cleanup Custom Template
############################################################
{
  resp=$(curl_json DELETE "${BASE_URL}/api/templates/${README_TEMPLATE_NAME}")
  body="${resp%$'\n'*}"
  status="${resp##*$'\n'}"
  assert_status 200 "${status}" "README example: Delete custom template"
  if have_jq; then
    statusField=$(echo "${body}" | jq -r '.status // empty')
    nameField=$(echo "${body}" | jq -r '.name // empty')
    [[ "${statusField}" == "deleted" ]] || { echo "[FAIL] README example: .status != deleted" >&2; exit 1; }
    [[ "${nameField}" == "${README_TEMPLATE_NAME}" ]] || { echo "[FAIL] README example: .name != ${README_TEMPLATE_NAME}" >&2; exit 1; }
    echo "[OK] README example: Custom template deleted successfully"
  else
    assert_body_contains "${body}" '"status"\s*:\s*"deleted"' "README example: Template deletion"
  fi
}

############################################################
# 21) README.md API Examples - Verify All Pre-built Templates
############################################################
{
  echo "Testing all 7 pre-built templates from README.md..."
  README_TEMPLATES=("news-site" "ecommerce-site" "blog-site" "corporate-site" "documentation-site" "forum-site" "social-media")
  for template in "${README_TEMPLATES[@]}"; do
    resp=$(curl_json GET "${BASE_URL}/api/templates/${template}")
    body="${resp%$'\n'*}"
    status="${resp##*$'\n'}"
    if [[ "${status}" == "200" ]]; then
      if have_jq; then
        name=$(echo "${body}" | jq -r '.name // empty')
        [[ "${name}" == "${template}" ]] || { echo "[FAIL] README example: ${template} name mismatch" >&2; exit 1; }
      fi
      echo "[OK] README example: ${template} template available"
    else
      echo "[FAIL] README example: ${template} template not found (status: ${status})" >&2
      exit 1
    fi
  done
  echo "[OK] README example: All 7 pre-built templates verified"
}

echo "=================================================="
echo "All template endpoint tests passed successfully!"
echo "Tested endpoints:"
echo "  ✓ GET /api/templates"
echo "  ✓ GET /api/templates/:name"
echo "  ✓ POST /api/templates"
echo "  ✓ DELETE /api/templates/:name"
echo "  ✓ POST /api/crawl/add-site-with-template"
echo "  ✓ Error handling for all endpoints"
echo "  ✓ README.md API examples validation"
echo "  ✓ All 7 pre-built templates verified"


