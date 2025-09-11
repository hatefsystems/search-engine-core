# 🎨 **Phase 3b: Frontend Job Dashboard**

## 📋 **Issue Description**
Create a responsive web dashboard for real-time job monitoring and management. This phase builds the user interface that connects to the real-time status system and provides full job management capabilities.

## 🎯 **Acceptance Criteria**
- [ ] Responsive job dashboard with real-time updates
- [ ] Active jobs panel with live progress indicators
- [ ] Job history browser with search and filtering
- [ ] Job management actions (cancel, retry, clone)
- [ ] Progressive enhancement (WebSocket → SSE → Polling)
- [ ] Mobile-friendly responsive design

## 📦 **Tasks**

### **HTML Templates**
- [ ] **Job Dashboard Template** (`templates/job-dashboard.html`)
  - Main dashboard layout with sidebar navigation
  - Active jobs panel with real-time status cards
  - Job history section with pagination
  - Job details modal/expandable sections
  - Action buttons and confirmation dialogs

- [ ] **Job Card Component Template** (`templates/components/job-card.html`)
  ```html
  <div class="job-card" data-job-id="{job_id}" data-job-status="{status}">
      <div class="job-header">
          <h3 class="job-title">{job_type}: {domain}</h3>
          <span class="job-status status-{status}">{status}</span>
      </div>
      <div class="job-progress">
          <div class="progress-bar">
              <div class="progress-fill" data-progress="{progress}"></div>
          </div>
          <span class="progress-text">{progress}% - {current_operation}</span>
      </div>
      <div class="job-actions">
          <button class="btn-cancel" data-action="cancel">Cancel</button>
          <button class="btn-retry" data-action="retry">Retry</button>
          <button class="btn-details" data-action="details">Details</button>
      </div>
  </div>
  ```

- [ ] **Job Details Modal Template** (`templates/components/job-details-modal.html`)
  - Detailed job information and configuration
  - Real-time log streaming section
  - Results preview and download links
  - Error details and troubleshooting info

### **CSS Styling** 
- [ ] **Job Dashboard Styles** (`public/css/job-dashboard.css`)
  - **Reuse existing CSS custom properties** from current design system
  - Responsive grid layout for job cards
  - Progress bar animations and status indicators
  - Modal dialogs and overlay styles
  - Mobile-responsive breakpoints

- [ ] **CSS Custom Properties Integration**
  ```css
  .job-dashboard {
      font-family: var(--font-family);
      padding: var(--space-4) 0;
      background: var(--background-color);
  }

  .job-card {
      border-radius: var(--border-radius);
      box-shadow: var(--shadow-sm);
      transition: var(--transition-default);
  }

  .progress-bar {
      background: var(--color-gray-200);
      border-radius: var(--border-radius-sm);
  }

  .progress-fill {
      background: var(--color-primary);
      transition: width 0.3s ease;
  }
  ```

- [ ] **Status-Specific Styling**
  ```css
  .status-queued { color: var(--color-gray-600); }
  .status-processing { color: var(--color-blue-600); }
  .status-completed { color: var(--color-green-600); }
  .status-failed { color: var(--color-red-600); }
  .status-cancelled { color: var(--color-orange-600); }
  ```

### **JavaScript Implementation**
- [ ] **JobDashboard Main Class** (`public/js/job-dashboard.js`)
  - Dashboard initialization and setup
  - Real-time connection management (WebSocket/SSE/Polling)
  - Job card rendering and updates
  - User interaction handling
  - **NO inline event handlers** (CSP compliance)

- [ ] **Real-time Connection Manager** (`public/js/realtime-connection.js`)
  ```javascript
  class RealtimeConnectionManager {
      constructor(userId, onStatusUpdate) {
          this.userId = userId;
          this.onStatusUpdate = onStatusUpdate;
          this.connectionMethod = null;
          this.reconnectAttempts = 0;
      }

      async connect() {
          // Try WebSocket first
          if (await this.tryWebSocket()) return;
          
          // Fallback to SSE
          if (await this.trySSE()) return;
          
          // Final fallback to polling
          this.startPolling();
      }

      tryWebSocket() {
          const ws = new WebSocket(`ws://${location.host}/ws/jobs`);
          ws.onopen = () => this.handleWebSocketOpen(ws);
          ws.onmessage = (event) => this.handleMessage(JSON.parse(event.data)); 
          ws.onclose = () => this.handleDisconnection();
          ws.onerror = () => this.handleError();
      }
  }
  ```

- [ ] **Job Management Actions** (`public/js/job-actions.js`)
  ```javascript
  class JobActions {
      static async cancelJob(jobId) {
          const response = await fetch(`/api/v2/jobs/${jobId}`, {
              method: 'DELETE'
          });
          return response.json();
      }

      static async retryJob(jobId) {
          const response = await fetch(`/api/v2/jobs/${jobId}/retry`, {
              method: 'PUT'
          });
          return response.json();
      }

      static async getJobDetails(jobId) {
          const response = await fetch(`/api/v2/jobs/${jobId}`);
          return response.json();
      }
  }
  ```

### **Dashboard Components**
- [ ] **Active Jobs Panel** (`public/js/components/active-jobs-panel.js`)
  - Real-time job status cards
  - Progress animations and updates
  - Live ETA calculations
  - Auto-refresh and sorting

- [ ] **Job History Browser** (`public/js/components/job-history.js`)
  - Paginated job history display
  - Search and filtering capabilities
  - Date range selection
  - Export functionality

- [ ] **Job Details Modal** (`public/js/components/job-details-modal.js`)
  - Detailed job information display
  - Real-time log streaming
  - Results preview and download
  - Error analysis and troubleshooting

## 🔧 **Technical Requirements**

### **Progressive Enhancement Strategy**
- [ ] **Base HTML/CSS** works without JavaScript
- [ ] **JavaScript Enhancement** adds real-time features
- [ ] **WebSocket Support** provides best experience
- [ ] **Graceful Degradation** to polling if needed

### **Event Handling (CSP Compliant)**
- [ ] **Data Attributes for Actions**
  ```html
  <!-- NO onclick handlers -->
  <button data-action="cancel" data-job-id="job_123">Cancel</button>
  <button data-action="retry" data-job-id="job_456">Retry</button>
  ```

- [ ] **Event Delegation Pattern**
  ```javascript
  // Attach listeners to container, not individual buttons
  document.querySelector('.job-dashboard').addEventListener('click', function(e) {
      const action = e.target.getAttribute('data-action');
      const jobId = e.target.getAttribute('data-job-id');
      
      if (action === 'cancel') {
          JobActions.cancelJob(jobId);
      } else if (action === 'retry') {
          JobActions.retryJob(jobId);
      }
  });
  ```

### **Responsive Design Requirements**
- [ ] **Mobile-First CSS**
  - Job cards stack vertically on mobile
  - Touch-friendly button sizes (44px minimum)
  - Collapsible sidebar navigation
  - Horizontal scrolling for job details

- [ ] **Breakpoint Strategy**
  ```css
  /* Mobile first */
  .job-grid {
      display: block;
  }

  /* Tablet and up */
  @media (min-width: 768px) {
      .job-grid {
          display: grid;
          grid-template-columns: repeat(2, 1fr);
          gap: var(--space-4);
      }
  }

  /* Desktop and up */
  @media (min-width: 1024px) {
      .job-grid {
          grid-template-columns: repeat(3, 1fr);
      }
  }
  ```

## 🧪 **Testing Strategy**

### **Frontend Unit Tests** (`tests/frontend/`)
- [ ] **JobDashboard.test.js**
  - Dashboard initialization
  - Real-time connection handling
  - Job card rendering and updates
  - User interaction handling

- [ ] **RealtimeConnection.test.js**
  - WebSocket connection logic
  - SSE fallback behavior
  - Polling fallback functionality
  - Reconnection logic

### **Integration Tests** (`tests/integration/frontend/`)
- [ ] **DashboardIntegrationTest.js**
  - Full dashboard workflow testing
  - Real-time updates end-to-end
  - Job management actions
  - Error handling scenarios

### **E2E Tests** (`tests/e2e/`)
- [ ] **JobDashboardE2E.test.js** (using Playwright or similar)
  ```javascript
  test('Job dashboard real-time updates', async ({ page }) => {
      await page.goto('/job-dashboard');
      
      // Submit a job via API
      const jobId = await submitTestJob();
      
      // Verify job appears in dashboard
      await expect(page.locator(`[data-job-id="${jobId}"]`)).toBeVisible();
      
      // Verify progress updates
      await expect(page.locator('.progress-fill')).toHaveAttribute('data-progress', '0');
      
      // Wait for progress update
      await expect(page.locator('.progress-fill')).toHaveAttribute('data-progress', /\d+/);
  });
  ```

## 📊 **Success Criteria**

### **Performance Targets**
- Dashboard load time: < 2 seconds
- Real-time update latency: < 1 second
- Smooth animations at 60 FPS
- Mobile performance: < 3 seconds on 3G

### **Usability Targets**
- Job status visible immediately upon dashboard load
- Progress updates smooth and informative
- All actions complete within 5 seconds
- Mobile interface fully functional

### **Accessibility Requirements**
- WCAG 2.1 AA compliance
- Keyboard navigation support
- Screen reader compatibility
- High contrast mode support

## 🔗 **Dependencies**
- **Requires**: Phase 3a (Real-time Status System)
- **Integrates**: Existing CSS design system
- **Uses**: Job API endpoints from Phase 2a

## 📝 **Implementation Notes**

### **Critical Implementation Rules**
- **ALWAYS** reuse existing CSS custom properties (DRY principle)
- **NEVER** use inline JavaScript event handlers (CSP compliance)
- **ALWAYS** use data attributes for dynamic content
- **ALWAYS** implement progressive enhancement
- **ALWAYS** test on mobile devices

### **CSS Reuse Strategy**
- Extend existing design system variables
- Reuse utility classes where possible
- Follow existing naming conventions
- Maintain consistent visual hierarchy

### **JavaScript Best Practices**
- Use modern ES6+ features (async/await, arrow functions)
- Implement proper error handling for network requests
- Use event delegation for dynamic content
- Optimize for performance (debouncing, throttling)

### **Real-time Update Strategy**
- Batch multiple updates to prevent UI thrashing
- Use CSS transitions for smooth progress animations
- Implement smart re-rendering to avoid flicker
- Cache frequently accessed DOM elements

## 🏷️ **Labels**
`phase-3b` `frontend` `dashboard` `real-time` `responsive` `javascript`

## ⏱️ **Estimated Timeline**
**6-8 days** for complete implementation and testing

## 📋 **Definition of Done**
- [ ] Responsive job dashboard implemented
- [ ] Real-time status updates working
- [ ] Job management actions functional
- [ ] Progressive enhancement working (WebSocket → SSE → Polling)
- [ ] Mobile-responsive design complete
- [ ] CSP-compliant JavaScript (no inline handlers)
- [ ] CSS reuses existing design system
- [ ] All frontend unit tests passing
- [ ] Integration tests working
- [ ] E2E tests covering main workflows
- [ ] Accessibility requirements met
- [ ] Performance targets achieved
- [ ] Cross-browser testing complete
- [ ] Code review completed

---
**Previous Phase**: Phase 3a - Real-time Status System  
**Next Phase**: Phase 4a - Advanced Job Types (Optional)
