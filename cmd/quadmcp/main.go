// quadmcp - MCP server for Quadrate language documentation
//
// Provides a Model Context Protocol (MCP) server that serves
// Quadrate standard library documentation via stdio.
//
// Usage:
//
//	quadmcp                    # Start stdio MCP server
//	quadmcp -http :8080        # Start HTTP server instead
//	quadmcp -docs /path/to/api # Custom API docs path
package main

import (
	"bufio"
	"encoding/json"
	"flag"
	"fmt"
	"io"
	"log"
	"net/http"
	"os"
	"path/filepath"
	"strings"

	"github.com/gin-gonic/gin"
)

// Module represents a Quadrate standard library module
type Module struct {
	Name        string     `json:"name"`
	Description string     `json:"description"`
	Constants   []Constant `json:"constants"`
	Structs     []Struct   `json:"structs"`
	Functions   []Function `json:"functions"`
}

// Constant represents a module constant
type Constant struct {
	Name        string `json:"name"`
	Value       string `json:"value"`
	Description string `json:"description"`
}

// Struct represents a module struct
type Struct struct {
	Name        string `json:"name"`
	Description string `json:"description"`
}

// Function represents a module function
type Function struct {
	Name        string   `json:"name"`
	Signature   string   `json:"signature"`
	Failable    bool     `json:"failable"`
	Description string   `json:"description"`
	Params      []Param  `json:"params"`
	Returns     []Param  `json:"returns"`
	Errors      []string `json:"errors"`
	Examples    []string `json:"examples"`
}

// Param represents a function parameter or return value
type Param struct {
	Name        string `json:"name"`
	Type        string `json:"type"`
	Description string `json:"description"`
}

// Instruction represents a builtin instruction
type Instruction struct {
	Name        string `json:"name"`
	Signature   string `json:"signature"`
	Description string `json:"description"`
	Alias       string `json:"alias,omitempty"`
}

// Category represents a category of builtin instructions
type Category struct {
	Name         string        `json:"name"`
	Instructions []Instruction `json:"instructions"`
}

// Builtins represents all builtin instructions
type Builtins struct {
	Name        string     `json:"name"`
	Description string     `json:"description"`
	Categories  []Category `json:"categories"`
}

// DocStore holds all loaded module documentation
type DocStore struct {
	modules  map[string]*Module
	builtins *Builtins
}

// NewDocStore creates a new documentation store
func NewDocStore() *DocStore {
	return &DocStore{
		modules: make(map[string]*Module),
	}
}

// LoadFromDir loads all JSON documentation files from a directory
func (ds *DocStore) LoadFromDir(dir string) error {
	entries, err := os.ReadDir(dir)
	if err != nil {
		return fmt.Errorf("failed to read docs directory: %w", err)
	}

	for _, entry := range entries {
		if entry.IsDir() || !strings.HasSuffix(entry.Name(), ".json") {
			continue
		}

		path := filepath.Join(dir, entry.Name())
		data, err := os.ReadFile(path)
		if err != nil {
			continue
		}

		// Handle builtins.json separately
		if entry.Name() == "builtins.json" {
			var builtins Builtins
			if err := json.Unmarshal(data, &builtins); err == nil {
				ds.builtins = &builtins
			}
			continue
		}

		var mod Module
		if err := json.Unmarshal(data, &mod); err != nil {
			continue
		}

		ds.modules[mod.Name] = &mod
	}

	return nil
}

// GetBuiltins returns the builtins documentation
func (ds *DocStore) GetBuiltins() *Builtins {
	return ds.builtins
}

// GetBuiltin returns a specific builtin instruction by name
func (ds *DocStore) GetBuiltin(name string) *Instruction {
	if ds.builtins == nil {
		return nil
	}
	for _, cat := range ds.builtins.Categories {
		for _, instr := range cat.Instructions {
			if instr.Name == name || instr.Alias == name {
				return &instr
			}
		}
	}
	return nil
}

// GetModule returns a module by name
func (ds *DocStore) GetModule(name string) *Module {
	return ds.modules[name]
}

// ListModules returns all module names
func (ds *DocStore) ListModules() []string {
	names := make([]string, 0, len(ds.modules))
	for name := range ds.modules {
		names = append(names, name)
	}
	return names
}

// SearchFunctions searches for functions matching a query
func (ds *DocStore) SearchFunctions(query string) []SearchResult {
	query = strings.ToLower(query)
	var results []SearchResult

	// Search builtins
	if ds.builtins != nil {
		for _, cat := range ds.builtins.Categories {
			for _, instr := range cat.Instructions {
				if strings.Contains(strings.ToLower(instr.Name), query) ||
					strings.Contains(strings.ToLower(instr.Description), query) ||
					strings.Contains(strings.ToLower(instr.Alias), query) {
					results = append(results, SearchResult{
						Module:      "builtin",
						Name:        instr.Name,
						Kind:        "instruction",
						Signature:   instr.Signature,
						Description: instr.Description,
					})
				}
			}
		}
	}

	// Search modules
	for modName, mod := range ds.modules {
		for _, fn := range mod.Functions {
			if strings.Contains(strings.ToLower(fn.Name), query) ||
				strings.Contains(strings.ToLower(fn.Description), query) {
				results = append(results, SearchResult{
					Module:      modName,
					Name:        fn.Name,
					Kind:        "function",
					Signature:   fn.Signature,
					Description: fn.Description,
				})
			}
		}

		for _, c := range mod.Constants {
			if strings.Contains(strings.ToLower(c.Name), query) ||
				strings.Contains(strings.ToLower(c.Description), query) {
				results = append(results, SearchResult{
					Module:      modName,
					Name:        c.Name,
					Kind:        "constant",
					Value:       c.Value,
					Description: c.Description,
				})
			}
		}
	}

	return results
}

// SearchResult represents a search result
type SearchResult struct {
	Module      string `json:"module"`
	Name        string `json:"name"`
	Kind        string `json:"kind"`
	Signature   string `json:"signature,omitempty"`
	Value       string `json:"value,omitempty"`
	Description string `json:"description"`
}

// MCP Protocol types

// MCPRequest represents an incoming MCP request
type MCPRequest struct {
	JSONRPC string          `json:"jsonrpc"`
	ID      interface{}     `json:"id"`
	Method  string          `json:"method"`
	Params  json.RawMessage `json:"params,omitempty"`
}

// MCPResponse represents an outgoing MCP response
type MCPResponse struct {
	JSONRPC string      `json:"jsonrpc"`
	ID      interface{} `json:"id"`
	Result  interface{} `json:"result,omitempty"`
	Error   *MCPError   `json:"error,omitempty"`
}

// MCPError represents an MCP error
type MCPError struct {
	Code    int    `json:"code"`
	Message string `json:"message"`
}

// Tool definitions for MCP
type Tool struct {
	Name        string      `json:"name"`
	Description string      `json:"description"`
	InputSchema InputSchema `json:"inputSchema"`
}

type InputSchema struct {
	Type       string              `json:"type"`
	Properties map[string]Property `json:"properties,omitempty"`
	Required   []string            `json:"required,omitempty"`
}

type Property struct {
	Type        string `json:"type"`
	Description string `json:"description"`
}

// StdioServer handles MCP over stdio
type StdioServer struct {
	docs   *DocStore
	reader *bufio.Reader
	writer io.Writer
}

// NewStdioServer creates a new stdio MCP server
func NewStdioServer(docs *DocStore) *StdioServer {
	return &StdioServer{
		docs:   docs,
		reader: bufio.NewReader(os.Stdin),
		writer: os.Stdout,
	}
}

// Run starts the stdio server loop
func (s *StdioServer) Run() error {
	for {
		line, err := s.reader.ReadString('\n')
		if err != nil {
			if err == io.EOF {
				return nil
			}
			return err
		}

		line = strings.TrimSpace(line)
		if line == "" {
			continue
		}

		var req MCPRequest
		if err := json.Unmarshal([]byte(line), &req); err != nil {
			s.sendError(nil, -32700, "Parse error")
			continue
		}

		resp := s.processRequest(&req)
		s.sendResponse(resp)
	}
}

func (s *StdioServer) sendResponse(resp MCPResponse) {
	data, _ := json.Marshal(resp)
	fmt.Fprintln(s.writer, string(data))
}

func (s *StdioServer) sendError(id interface{}, code int, message string) {
	resp := MCPResponse{
		JSONRPC: "2.0",
		ID:      id,
		Error:   &MCPError{Code: code, Message: message},
	}
	s.sendResponse(resp)
}

func (s *StdioServer) processRequest(req *MCPRequest) MCPResponse {
	switch req.Method {
	case "initialize":
		return MCPResponse{
			JSONRPC: "2.0",
			ID:      req.ID,
			Result: map[string]interface{}{
				"protocolVersion": "2024-11-05",
				"capabilities": map[string]interface{}{
					"tools": map[string]interface{}{},
				},
				"serverInfo": map[string]interface{}{
					"name":    "quadmcp",
					"version": "1.0.0",
				},
			},
		}

	case "notifications/initialized":
		// No response needed for notifications
		return MCPResponse{JSONRPC: "2.0", ID: req.ID, Result: map[string]interface{}{}}

	case "tools/list":
		tools := []Tool{
			{
				Name:        "quadrate_list_modules",
				Description: "List all available Quadrate standard library modules",
				InputSchema: InputSchema{Type: "object"},
			},
			{
				Name:        "quadrate_get_module",
				Description: "Get documentation for a specific Quadrate module including all functions and constants",
				InputSchema: InputSchema{
					Type: "object",
					Properties: map[string]Property{
						"name": {Type: "string", Description: "Module name (e.g., 'math', 'str', 'io')"},
					},
					Required: []string{"name"},
				},
			},
			{
				Name:        "quadrate_get_function",
				Description: "Get detailed documentation for a specific Quadrate function",
				InputSchema: InputSchema{
					Type: "object",
					Properties: map[string]Property{
						"module":   {Type: "string", Description: "Module name"},
						"function": {Type: "string", Description: "Function name"},
					},
					Required: []string{"module", "function"},
				},
			},
			{
				Name:        "quadrate_search",
				Description: "Search Quadrate standard library for functions and constants by name or description",
				InputSchema: InputSchema{
					Type: "object",
					Properties: map[string]Property{
						"query": {Type: "string", Description: "Search query"},
					},
					Required: []string{"query"},
				},
			},
			{
				Name:        "quadrate_get_builtins",
				Description: "Get all Quadrate builtin instructions organized by category",
				InputSchema: InputSchema{Type: "object"},
			},
			{
				Name:        "quadrate_get_builtin",
				Description: "Get documentation for a specific Quadrate builtin instruction",
				InputSchema: InputSchema{
					Type: "object",
					Properties: map[string]Property{
						"name": {Type: "string", Description: "Instruction name (e.g., 'dup', 'swap', 'add', '+')"},
					},
					Required: []string{"name"},
				},
			},
		}
		return MCPResponse{
			JSONRPC: "2.0",
			ID:      req.ID,
			Result:  map[string]interface{}{"tools": tools},
		}

	case "tools/call":
		return s.handleToolCall(req)

	default:
		return MCPResponse{
			JSONRPC: "2.0",
			ID:      req.ID,
			Error:   &MCPError{Code: -32601, Message: "Method not found"},
		}
	}
}

func (s *StdioServer) handleToolCall(req *MCPRequest) MCPResponse {
	var params struct {
		Name      string          `json:"name"`
		Arguments json.RawMessage `json:"arguments"`
	}
	if err := json.Unmarshal(req.Params, &params); err != nil {
		return MCPResponse{
			JSONRPC: "2.0",
			ID:      req.ID,
			Error:   &MCPError{Code: -32602, Message: "Invalid params"},
		}
	}

	var result interface{}
	var errMsg string

	switch params.Name {
	case "quadrate_list_modules":
		modules := s.docs.ListModules()
		var moduleInfos []map[string]interface{}
		for _, name := range modules {
			mod := s.docs.GetModule(name)
			moduleInfos = append(moduleInfos, map[string]interface{}{
				"name":        mod.Name,
				"description": mod.Description,
				"functions":   len(mod.Functions),
				"constants":   len(mod.Constants),
			})
		}
		result = moduleInfos

	case "quadrate_get_module":
		var args struct {
			Name string `json:"name"`
		}
		json.Unmarshal(params.Arguments, &args)
		mod := s.docs.GetModule(args.Name)
		if mod == nil {
			errMsg = fmt.Sprintf("Module '%s' not found", args.Name)
		} else {
			result = mod
		}

	case "quadrate_get_function":
		var args struct {
			Module   string `json:"module"`
			Function string `json:"function"`
		}
		json.Unmarshal(params.Arguments, &args)
		mod := s.docs.GetModule(args.Module)
		if mod == nil {
			errMsg = fmt.Sprintf("Module '%s' not found", args.Module)
		} else {
			for _, fn := range mod.Functions {
				if fn.Name == args.Function {
					result = fn
					break
				}
			}
			if result == nil {
				errMsg = fmt.Sprintf("Function '%s' not found in module '%s'", args.Function, args.Module)
			}
		}

	case "quadrate_search":
		var args struct {
			Query string `json:"query"`
		}
		json.Unmarshal(params.Arguments, &args)
		result = s.docs.SearchFunctions(args.Query)

	case "quadrate_get_builtins":
		builtins := s.docs.GetBuiltins()
		if builtins == nil {
			errMsg = "Builtins documentation not available"
		} else {
			result = builtins
		}

	case "quadrate_get_builtin":
		var args struct {
			Name string `json:"name"`
		}
		json.Unmarshal(params.Arguments, &args)
		instr := s.docs.GetBuiltin(args.Name)
		if instr == nil {
			errMsg = fmt.Sprintf("Builtin instruction '%s' not found", args.Name)
		} else {
			result = instr
		}

	default:
		return MCPResponse{
			JSONRPC: "2.0",
			ID:      req.ID,
			Error:   &MCPError{Code: -32602, Message: fmt.Sprintf("Unknown tool: %s", params.Name)},
		}
	}

	if errMsg != "" {
		return MCPResponse{
			JSONRPC: "2.0",
			ID:      req.ID,
			Result: map[string]interface{}{
				"content": []map[string]interface{}{
					{"type": "text", "text": errMsg},
				},
				"isError": true,
			},
		}
	}

	text, _ := json.MarshalIndent(result, "", "  ")
	return MCPResponse{
		JSONRPC: "2.0",
		ID:      req.ID,
		Result: map[string]interface{}{
			"content": []map[string]interface{}{
				{"type": "text", "text": string(text)},
			},
		},
	}
}

// HTTPServer for optional HTTP mode
type HTTPServer struct {
	docs   *DocStore
	router *gin.Engine
}

func NewHTTPServer(docs *DocStore) *HTTPServer {
	gin.SetMode(gin.ReleaseMode)
	router := gin.New()
	router.Use(gin.Recovery())

	s := &HTTPServer{docs: docs, router: router}
	s.setupRoutes()
	return s
}

func (s *HTTPServer) setupRoutes() {
	s.router.GET("/health", func(c *gin.Context) {
		c.JSON(http.StatusOK, gin.H{"status": "ok"})
	})

	api := s.router.Group("/api")
	{
		api.GET("/modules", func(c *gin.Context) {
			modules := s.docs.ListModules()
			var result []gin.H
			for _, name := range modules {
				mod := s.docs.GetModule(name)
				result = append(result, gin.H{
					"name":        mod.Name,
					"description": mod.Description,
					"functions":   len(mod.Functions),
					"constants":   len(mod.Constants),
				})
			}
			c.JSON(http.StatusOK, result)
		})

		api.GET("/modules/:name", func(c *gin.Context) {
			name := c.Param("name")
			mod := s.docs.GetModule(name)
			if mod == nil {
				c.JSON(http.StatusNotFound, gin.H{"error": "Module not found"})
				return
			}
			c.JSON(http.StatusOK, mod)
		})

		api.GET("/search", func(c *gin.Context) {
			query := c.Query("q")
			if query == "" {
				c.JSON(http.StatusBadRequest, gin.H{"error": "Missing query parameter 'q'"})
				return
			}
			results := s.docs.SearchFunctions(query)
			c.JSON(http.StatusOK, results)
		})
	}
}

func (s *HTTPServer) Run(addr string) error {
	log.Printf("Starting HTTP server on %s", addr)
	return s.router.Run(addr)
}

func findDocsDir() string {
	exe, _ := os.Executable()
	exeDir := filepath.Dir(exe)

	candidates := []string{
		filepath.Join(exeDir, "..", "..", "docs", "api"),
		filepath.Join(exeDir, "..", "share", "quadrate", "docs", "api"),
		"docs/api",
		"/usr/share/quadrate/docs/api",
	}

	for _, dir := range candidates {
		if _, err := os.Stat(dir); err == nil {
			abs, _ := filepath.Abs(dir)
			return abs
		}
	}

	return "docs/api"
}

func main() {
	httpAddr := flag.String("http", "", "Run HTTP server on address (e.g., :8080)")
	docsDir := flag.String("docs", "", "Path to API docs directory (default: auto-detect)")
	flag.Parse()

	if *docsDir == "" {
		*docsDir = findDocsDir()
	}

	docs := NewDocStore()
	if err := docs.LoadFromDir(*docsDir); err != nil {
		fmt.Fprintf(os.Stderr, "Failed to load documentation: %v\n", err)
		os.Exit(1)
	}

	if len(docs.modules) == 0 {
		fmt.Fprintf(os.Stderr, "No documentation loaded from %s\n", *docsDir)
		os.Exit(1)
	}

	if *httpAddr != "" {
		// HTTP mode
		server := NewHTTPServer(docs)
		if err := server.Run(*httpAddr); err != nil {
			fmt.Fprintf(os.Stderr, "Server error: %v\n", err)
			os.Exit(1)
		}
	} else {
		// Stdio MCP mode (default)
		server := NewStdioServer(docs)
		if err := server.Run(); err != nil {
			fmt.Fprintf(os.Stderr, "Server error: %v\n", err)
			os.Exit(1)
		}
	}
}
