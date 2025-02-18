CXXFLAGS = -g -I./ -L./vendor/glfw/lib/ -lglfw3 -framework Cocoa -framework IOKit

demo: solver.o glad.o

glad.o: ./vendor/glad/glad.c
	cc -c $^ -o $@

clean:
	rm -f *.o
	rm -f demo