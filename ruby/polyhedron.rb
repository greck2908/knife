
require 'triangle'

class Polyhedron

 attr_reader :exterior, :interior
 attr_reader :center, :diameter

 def initialize(n0,n1,n2,n3)
  @exterior = [ Triangle.new(n1,n3,n2),
                Triangle.new(n0,n2,n3),
                Triangle.new(n0,n3,n1),
                Triangle.new(n0,n1,n2)]
  @interior = Array.new
  @center = [ (n0[0]+n1[0]+n2[0]+n3[0])*0.25,
              (n0[1]+n1[1]+n2[1]+n3[1])*0.25,
              (n0[2]+n1[2]+n2[2]+n3[2])*0.25 ]
  l2 = (n0[0]-@center[0])**2 + 
       (n0[1]-@center[1])**2 + 
       (n0[2]-@center[2])**2 
  d2 = (n1[0]-@center[0])**2 + 
       (n1[1]-@center[1])**2 + 
       (n1[2]-@center[2])**2
  l2 = d2 if d2 > l2
  d2 = (n2[0]-@center[0])**2 + 
       (n2[1]-@center[1])**2 + 
       (n2[2]-@center[2])**2
  l2 = d2 if d2 > l2
  d2 = (n3[0]-@center[0])**2 + 
       (n3[1]-@center[1])**2 + 
       (n3[2]-@center[2])**2
  l2 = d2 if d2 > l2
  @diameter = 1.0001*Math::sqrt(l2)
 end

 def cut_into(triangle)
  @exterior.each do |face|
   triangle.cut_with(face)
  end
 end

end