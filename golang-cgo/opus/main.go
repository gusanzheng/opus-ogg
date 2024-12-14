package main

/*
#cgo CFLAGS: -I .
#cgo LDFLAGS: -L . -lopus_codec
#include "interface.h"
*/
import "C"
import (
	"flag"
	"fmt"
	"io"
	"os"
	"unsafe"
)

type opusInst struct {
	inst unsafe.Pointer
}

func main() {
	fmt.Println(">>> START <<<")
	var (
		mode           string
		m              string
		inputFileName  string
		i              string
		outputFileName string
		o              string
	)

	flag.StringVar(&mode, "mode", "", "encode or decode")
	flag.StringVar(&m, "m", "default", "encode or decode")
	flag.StringVar(&inputFileName, "inputFileName", "", "输入文件")
	flag.StringVar(&i, "i", "default", "输入文件")
	flag.StringVar(&outputFileName, "outputFileName", "", "输出文件")
	flag.StringVar(&o, "o", "default", "输出文件")
	flag.Parse()

	if m != "default" {
		mode = m
	}
	if i != "default" {
		inputFileName = i
	}
	if o != "default" {
		outputFileName = o
	}

	fmt.Println("Params:", mode, inputFileName, outputFileName)

	oi := &opusInst{}
	cIntSampleRate := C.int(24000)
	retC := C.OpusCodecStart(&(oi.inst), cIntSampleRate)
	if retC != 0 {
		fmt.Println("Start error ", retC)
		return
	}

	inputFile, err := os.Open(inputFileName)
	if err != nil {
		fmt.Println("Error opening input file:", err)
		return
	}
	defer inputFile.Close()

	outputFile, err := os.Create(outputFileName)
	if err != nil {
		fmt.Println("Error creating output file:", err)
		return
	}
	defer outputFile.Close()

	var outputBuffer []byte
	buffer := make([]byte, 4096)
	for {
		bytesRead, err := inputFile.Read(buffer)
		if err == io.EOF {
			break
		}
		if err != nil {
			fmt.Println("Error reading input file:", err)
			return
		}

		cInput := C.CString(string(buffer[:bytesRead]))
		defer C.free(unsafe.Pointer(cInput))

		var cOutput *C.char
		var cOutputLen C.int
		last := C.bool(false)

		if bytesRead < 4096 {
			last = C.bool(true)
		}

		fmt.Println(">>>", bytesRead)
		switch mode {
		case "encode":
			result := C.OpusCodecEncode(oi.inst, cInput, C.int(bytesRead), &cOutput, &cOutputLen, last)
			if result != 0 {
				fmt.Println("Encoding failed.")
				return
			}
		case "decode":
			result := C.OpusCodecDecode(oi.inst, cInput, C.int(bytesRead), &cOutput, &cOutputLen, last)
			if result != 0 {
				fmt.Println("Decoding failed.")
				return
			}
		default:
			fmt.Println("Invalid mode.")
			return
		}
		outputSlice := C.GoBytes(unsafe.Pointer(cOutput), cOutputLen)
		outputBuffer = append(outputBuffer, outputSlice...)
		C.free(unsafe.Pointer(cOutput))
	}

	// Write the processed output to the output file
	_, err = outputFile.Write(outputBuffer)
	if err != nil {
		fmt.Println("Error writing to output file:", err)
		return
	}

	retC = C.OpusCodecEnd(&(oi.inst))
	if retC != 0 {
		fmt.Println("End error ", retC)
		return
	}

	fmt.Println(">>> FINISH <<<")
}
