package main

import (
	"bytes"
	"context"
	"embed"
	"encoding/json"
	"flag"
	"fmt"
	"html/template"
	"log"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"time"
)

//go:embed templates/*
var templates embed.FS

var (
	addr         string
	sandboxImage string
	timeout      time.Duration
)

type RunRequest struct {
	Code string `json:"code"`
}

type RunResponse struct {
	Output string `json:"output,omitempty"`
	Error  string `json:"error,omitempty"`
}

func main() {
	flag.StringVar(&addr, "addr", ":8080", "listen address")
	flag.StringVar(&sandboxImage, "image", "quadrate-sandbox", "Docker sandbox image")
	flag.DurationVar(&timeout, "timeout", 5*time.Second, "execution timeout")
	flag.Parse()

	// Verify Docker is available
	if err := exec.Command("docker", "version").Run(); err != nil {
		log.Fatalf("Docker not available: %v", err)
	}

	// Verify sandbox image exists
	if err := exec.Command("docker", "image", "inspect", sandboxImage).Run(); err != nil {
		log.Fatalf("Sandbox image '%s' not found. Build it with: tools/playground/build-sandbox.sh", sandboxImage)
	}

	tmpl, err := template.ParseFS(templates, "templates/*.html")
	if err != nil {
		log.Fatalf("failed to parse templates: %v", err)
	}

	http.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) {
		if r.URL.Path != "/" {
			http.NotFound(w, r)
			return
		}
		tmpl.ExecuteTemplate(w, "index.html", nil)
	})

	http.HandleFunc("/favicon.ico", func(w http.ResponseWriter, r *http.Request) {
		data, _ := templates.ReadFile("templates/favicon.ico")
		w.Header().Set("Content-Type", "image/x-icon")
		w.Write(data)
	})

	http.HandleFunc("/run", handleRun)

	log.Printf("Quadrate Playground listening on %s (image: %s)", addr, sandboxImage)
	log.Fatal(http.ListenAndServe(addr, nil))
}

func handleRun(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
		return
	}

	var req RunRequest
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		writeJSON(w, http.StatusBadRequest, RunResponse{Error: "invalid request"})
		return
	}

	if len(req.Code) > 64*1024 {
		writeJSON(w, http.StatusBadRequest, RunResponse{Error: "code exceeds 64KB limit"})
		return
	}

	if req.Code == "" {
		writeJSON(w, http.StatusBadRequest, RunResponse{Error: "no code provided"})
		return
	}

	output, err := runCode(req.Code)
	if err != nil {
		writeJSON(w, http.StatusOK, RunResponse{Error: err.Error(), Output: output})
		return
	}

	writeJSON(w, http.StatusOK, RunResponse{Output: output})
}

func runCode(code string) (string, error) {
	tmpDir, err := os.MkdirTemp("/var/lib/playground", "run-*")
	if err != nil {
		return "", fmt.Errorf("failed to create temp directory: %v", err)
	}
	defer os.RemoveAll(tmpDir)

	srcFile := filepath.Join(tmpDir, "main.qd")
	if err := os.WriteFile(srcFile, []byte(code), 0644); err != nil {
		return "", fmt.Errorf("failed to write source: %v", err)
	}

	os.Chmod(srcFile, 0644)
	os.Chmod(tmpDir, 0755)

	log.Printf("Running code from %s", srcFile)

	// Verify file exists
	if _, err := os.Stat(srcFile); err != nil {
		log.Printf("ERROR: source file does not exist: %v", err)
	}

	ctx, cancel := context.WithTimeout(context.Background(), timeout)
	defer cancel()

	args := []string{"run",
		"--rm",
		"--network", "none",
		"--read-only",
		"--tmpfs", "/tmp:rw,exec,nosuid,size=16m",
		"--memory", "64m",
		"--cpus", "0.5",
		"--pids-limit", "32",
		"--security-opt", "no-new-privileges",
		"--cap-drop", "ALL",
		"-v", tmpDir + ":/sandbox:ro",
		"-e", "NO_COLOR=1",
		sandboxImage,
		"-r", "/sandbox/main.qd",
	}
	log.Printf("Docker command: docker %v", args)

	cmd := exec.CommandContext(ctx, "docker", args...)

	var stdout, stderr bytes.Buffer
	cmd.Stdout = &stdout
	cmd.Stderr = &stderr

	err = cmd.Run()

	output := stdout.String()
	if stderr.Len() > 0 {
		if output != "" {
			output += "\n"
		}
		output += stderr.String()
	}

	if ctx.Err() == context.DeadlineExceeded {
		return output, fmt.Errorf("execution timed out after %v", timeout)
	}

	if err != nil {
		if output == "" {
			return "", fmt.Errorf("execution failed: %v", err)
		}
		return output, fmt.Errorf("execution failed")
	}

	return output, nil
}

func writeJSON(w http.ResponseWriter, status int, v any) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(status)
	json.NewEncoder(w).Encode(v)
}
